#include "chrome/browser/ui/webui/whister/whister_settings_handler.h"
#include <array>
#include <algorithm>
#include <string_view>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "base/files/file_util.h"
#include "base/files/file.h"
#include "base/command_line.h"
#include "base/process/launch.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/prefs/pref_service.h"
#include "crypto/secure_hash.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "v8/include/v8-version-string.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "base/json/json_reader.h"

namespace whister {

    namespace {

    constexpr char kUpdateManifestUrl[] =
        "https://beta-updates.whisterbrowser.com/win/beta/manifest.json";
    constexpr char kUpdateHost[] = "beta-updates.whisterbrowser.com";
    constexpr int64_t kMaximumInstallerSize = 1024LL * 1024 * 1024;
    constexpr size_t kMaximumBookmarks = 12;

    bool IsValidSha256(std::string_view value) {
        return value.size() == 64 &&
               std::all_of(value.begin(), value.end(),
                           [](char character) {
                               return base::IsHexDigit(character);
                           });
    }

    void AppendBookmarks(const bookmarks::BookmarkNode* folder,
                         base::ListValue* output,
                         size_t depth = 0) {
        if (depth > 16) {
            return;
        }
        for (const auto& child : folder->children()) {
            if (output->size() >= kMaximumBookmarks) {
                return;
            }
            if (child->is_url()) {
                const GURL& url = child->url();
                if (!url.SchemeIsHTTPOrHTTPS()) {
                    continue;
                }
                base::DictValue bookmark;
                bookmark.Set("title", base::UTF16ToUTF8(child->GetTitle()));
                bookmark.Set("url", url.spec());
                output->Append(std::move(bookmark));
            } else {
                AppendBookmarks(child.get(), output, depth + 1);
            }
        }
    }

    std::optional<std::string> HashFileSha256(const base::FilePath& path) {
        base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
        if (!file.IsValid()) {
            return std::nullopt;
        }

        auto hash = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
        std::array<uint8_t, 64 * 1024> buffer;
        while (true) {
            const std::optional<size_t> bytes_read = file.ReadAtCurrentPos(buffer);
            if (!bytes_read.has_value()) {
                return std::nullopt;
            }
            if (*bytes_read == 0) {
                break;
            }
            hash->Update(base::span(buffer).first(*bytes_read));
        }

        std::array<uint8_t, 32> digest;
        hash->Finish(digest);
        return base::HexEncodeLower(digest);
    }

            enum class Type { kBool, kString, kInt };

        struct Setting {
            const char* key;
            Type type;
            int min = 0;
            int max = 0;
        };

        // Only prefs listed here can be touched from the page.
        constexpr Setting kSettings[] = {
            { "browser.show_home_button", Type::kBool },
            { "homepage_is_newtabpage", Type::kBool },
            { "homepage", Type::kString },
            { "session.restore_on_startup", Type::kInt, 1, 5 },
            { "enable_do_not_track", Type::kBool },
            { "search.suggest_enabled", Type::kBool },
            { "alternate_error_pages.enabled", Type::kBool },
            { "download.prompt_for_download", Type::kBool },
            { "download.open_pdf_in_system_reader", Type::kBool },
            { "browser.theme.color_scheme2", Type::kInt, 0, 2 },
            { "bookmark_bar.show_on_all_tabs", Type::kBool },
            { "webkit.webprefs.default_font_size", Type::kInt, 9, 40 },
            { "webkit.webprefs.default_fixed_font_size", Type::kInt, 9, 40 },
            { "webkit.webprefs.minimum_font_size", Type::kInt, 0, 24 },
            { "browser.enable_spellchecking", Type::kBool },
            { "translate.enabled", Type::kBool },
            { "autofill.profile_enabled", Type::kBool },
            { "autofill.credit_card_enabled", Type::kBool },
            { "credentials_enable_service", Type::kBool },
            { "safebrowsing.enabled", Type::kBool },
        };

        const Setting* FindSetting(const std::string& key) {
            for (const Setting& setting : kSettings) {
                if (key == setting.key) {
                    return &setting;
                }
            }
            return nullptr;
        }

    }  // namespace

    WhisterSettingsHandler::WhisterSettingsHandler() = default;
    WhisterSettingsHandler::~WhisterSettingsHandler() = default;

    void WhisterSettingsHandler::RegisterMessages() {
        web_ui()->RegisterMessageCallback(
            "getSettings",
            base::BindRepeating(&WhisterSettingsHandler::HandleGetSettings,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "setSetting",
            base::BindRepeating(&WhisterSettingsHandler::HandleSetSetting,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "getAboutInfo",
            base::BindRepeating(&WhisterSettingsHandler::HandleGetAboutInfo,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "getSearchEngines",
            base::BindRepeating(&WhisterSettingsHandler::HandleGetSearchEngines,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "setSearchEngine",
            base::BindRepeating(&WhisterSettingsHandler::HandleSetSearchEngine,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "navigateToUrl",
            base::BindRepeating(&WhisterSettingsHandler::HandleNavigateToUrl,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "checkWhisterUpdates",
            base::BindRepeating(&WhisterSettingsHandler::CheckForUpdates,
                base::Unretained(this)));

        web_ui()->RegisterMessageCallback(
            "downloadAndVerifyWhisterUpdate",
            base::BindRepeating(
                &WhisterSettingsHandler::DownloadAndVerifyUpdate,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "installVerifiedWhisterUpdate",
            base::BindRepeating(
                &WhisterSettingsHandler::InstallVerifiedUpdate,
                base::Unretained(this)));
        web_ui()->RegisterMessageCallback(
            "getBookmarks",
            base::BindRepeating(&WhisterSettingsHandler::HandleGetBookmarks,
                base::Unretained(this)));
    }

    void WhisterSettingsHandler::HandleNavigateToUrl(
        const base::ListValue& args) {
        if (args.size() != 1 || !args[0].is_string() ||
            args[0].GetString().size() > 4096) {
            return;
        }
        GURL url(args[0].GetString());
        const bool allowed_internal_page =
            (url.SchemeIs("chrome") &&
             (url.host() == "whister-settings" ||
              (url.host() == "settings" && url.path() == "/"))) ||
            (url.SchemeIs("chrome-extension") &&
             url.host() == "hoganoibjngfmjnnniladdmgmkkigmaj" &&
             url.path() == "/dashboard.html") ||
            (url.SchemeIs("whister") &&
             (url.host() == "settings" || url.host() == "newtab" ||
              url.host() == "setup" || url.host() == "whister-settings" ||
              url.host() == "whister-setup"));
        if (!url.is_valid() ||
            (!url.SchemeIsHTTPOrHTTPS() && !allowed_internal_page)) {
            return;
        }
        web_ui()->GetWebContents()->OpenURL(
            content::OpenURLParams::CreateBrowserInitiated(
                url, WindowOpenDisposition::kCurrentTab,
                ui::PAGE_TRANSITION_TYPED),
            /*navigation_handle_callback=*/{});
    }

    void WhisterSettingsHandler::HandleGetAboutInfo(const base::ListValue& args) {
        base::DictValue info;
        info.Set("chromium", std::string(version_info::GetVersionNumber()));
        info.Set("revision", std::string(version_info::GetLastChange()));
        info.Set("v8", std::string(V8_VERSION_STRING));
        info.Set("os", std::string(version_info::GetOSType()));
        info.Set("arch", sizeof(void*) == 8 ? "64-bit" : "32-bit");
        info.Set("build", version_info::IsOfficialBuild() ? "Official" : "Developer");
        info.Set("profile",
            Profile::FromWebUI(web_ui())->GetPath().AsUTF8Unsafe());
        web_ui()->CallJavascriptFunctionUnsafe("whisterSettings.onAbout", info);
    }

    void WhisterSettingsHandler::HandleGetSearchEngines(
        const base::ListValue& args) {
        SendSearchEngines();
    }

    void WhisterSettingsHandler::HandleSetSearchEngine(
        const base::ListValue& args) {
        if (args.size() != 1 || !args[0].is_int()) {
            return;
        }
        TemplateURLService* service =
            TemplateURLServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
        if (service && service->loaded()) {
            if (!list_controller_) {
                list_controller_ = std::make_unique<KeywordEditorController>(
                    Profile::FromWebUI(web_ui()));
            }
            TemplateURL* engine = list_controller_->GetTemplateURL(TemplateURLID(static_cast<long long>(args[0].GetInt())));
            if (engine && list_controller_->CanMakeDefault(engine)) {
                list_controller_->MakeDefaultTemplateURL(
                    engine->id(), search_engines::ChoiceMadeLocation::kSearchSettings);
            }
        }
        SendSearchEngines();
    }

    void WhisterSettingsHandler::SendSearchEngines() {
        TemplateURLService* service =
            TemplateURLServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
        if (!service) {
            return;
        }
        if (!service->loaded()) {
            loaded_subscription_ = service->RegisterOnLoadedCallback(base::BindOnce(
                &WhisterSettingsHandler::SendSearchEngines, base::Unretained(this)));
            service->Load();
            return;
        }
        const TemplateURL* current = service->GetDefaultSearchProvider();
        base::ListValue engines;
        for (TemplateURL* engine : service->GetTemplateURLs()) {
            if (!service->ShowInDefaultList(engine)) {
                continue;
            }
            base::DictValue entry;
            entry.Set("id", static_cast<int>(engine->id().value()));
            entry.Set("keyword", base::UTF16ToUTF8(engine->keyword()));
            entry.Set("name", base::UTF16ToUTF8(engine->short_name()));
            entry.Set("selected", engine == current);
            engines.Append(std::move(entry));
        }
        web_ui()->CallJavascriptFunctionUnsafe("whisterSettings.onSearchEngines",
            engines);
    }

    void WhisterSettingsHandler::HandleGetSettings(const base::ListValue& args) {
        SendSettings();
    }

    void WhisterSettingsHandler::HandleSetSetting(const base::ListValue& args) {
        if (args.size() != 2 || !args[0].is_string()) {
            return;
        }
        const std::string& key = args[0].GetString();
        const Setting* setting = FindSetting(key);
        if (!setting) {
            return;
        }

        PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
        const base::Value& value = args[1];
        switch (setting->type) {
        case Type::kBool:
            if (value.is_bool()) {
                prefs->SetBoolean(key, value.GetBool());
            }
            break;
        case Type::kString:
            if (value.is_string()) {
                GURL url(value.GetString());
                if (value.GetString().size() <= 2048 && url.is_valid() &&
                    url.SchemeIsHTTPOrHTTPS()) {
                    prefs->SetString(key, url.spec());
                }
            }
            break;
        case Type::kInt:
            if (value.is_int() && value.GetInt() >= setting->min &&
                value.GetInt() <= setting->max &&
                (key != "session.restore_on_startup" || value.GetInt() == 1 ||
                    value.GetInt() == 5)) {
                prefs->SetInteger(key, value.GetInt());
            }
            break;
        }
        // Echo the stored values so the page reverts rejected input.
        SendSettings();
    }

    void WhisterSettingsHandler::SendSettings() {
        PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
        base::DictValue out;
        for (const Setting& setting : kSettings) {
            switch (setting.type) {
            case Type::kBool:
                out.Set(setting.key, prefs->GetBoolean(setting.key));
                break;
            case Type::kString:
                out.Set(setting.key, prefs->GetString(setting.key));
                break;
            case Type::kInt:
                out.Set(setting.key, prefs->GetInteger(setting.key));
                break;
            }
        }
        web_ui()->CallJavascriptFunctionUnsafe("whisterSettings.onSettings", out);
    }

    void WhisterSettingsHandler::HandleGetBookmarks(
        const base::ListValue& args) {
        AllowJavascript();
        bookmarks::BookmarkModel* model =
            BookmarkModelFactory::GetForBrowserContext(
                Profile::FromWebUI(web_ui()));
        if (!model) {
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onBookmarks",
                base::Value(base::ListValue()));
            return;
        }
        if (!model->loaded()) {
            bookmarks_request_pending_ = true;
            bookmark_model_observation_.Observe(model);
            return;
        }
        SendBookmarks();
    }

    void WhisterSettingsHandler::SendBookmarks() {
        bookmarks::BookmarkModel* model =
            BookmarkModelFactory::GetForBrowserContext(
                Profile::FromWebUI(web_ui()));
        base::ListValue bookmarks;
        if (model && model->loaded()) {
            AppendBookmarks(model->bookmark_bar_node(), &bookmarks);
            if (bookmarks.size() < kMaximumBookmarks) {
                AppendBookmarks(model->other_node(), &bookmarks);
            }
            if (bookmarks.size() < kMaximumBookmarks) {
                AppendBookmarks(model->mobile_node(), &bookmarks);
            }
            if (bookmarks.size() < kMaximumBookmarks &&
                model->account_bookmark_bar_node()) {
                AppendBookmarks(model->account_bookmark_bar_node(), &bookmarks);
            }
            if (bookmarks.size() < kMaximumBookmarks &&
                model->account_other_node()) {
                AppendBookmarks(model->account_other_node(), &bookmarks);
            }
            if (bookmarks.size() < kMaximumBookmarks &&
                model->account_mobile_node()) {
                AppendBookmarks(model->account_mobile_node(), &bookmarks);
            }
        }
        web_ui()->CallJavascriptFunctionUnsafe(
            "whisterSettings.onBookmarks", bookmarks);
    }

    void WhisterSettingsHandler::BookmarkModelLoaded(bool ids_reassigned) {
        if (!bookmarks_request_pending_) {
            return;
        }
        bookmarks_request_pending_ = false;
        bookmark_model_observation_.Reset();
        SendBookmarks();
    }

    void WhisterSettingsHandler::OnJavascriptDisallowed() {
        bookmarks_request_pending_ = false;
        bookmark_model_observation_.Reset();
    }

    void WhisterSettingsHandler::CheckForUpdates(
        const base::ListValue& args) {
        available_update_.reset();
        if (!verified_installer_path_.empty()) {
            base::DeleteFile(verified_installer_path_);
        }
        verified_installer_path_ = base::FilePath();
        auto request = std::make_unique<network::ResourceRequest>();
        request->url = GURL(kUpdateManifestUrl);
        request->method = "GET";
        request->credentials_mode = network::mojom::CredentialsMode::kOmit;
        request->redirect_mode = network::mojom::RedirectMode::kError;

        url_loader_ = network::SimpleURLLoader::Create(
            std::move(request),
            net::DefineNetworkTrafficAnnotation("whister_update_manifest", R"(
              semantics {
                sender: "Whister Browser Update Check"
                description: "Checks the beta update manifest for a newer release."
                trigger: "The user selects Check for updates in Whister Settings."
                data: "No user data. The request retrieves public release metadata."
                destination: OTHER
              }
              policy {
                cookies_allowed: NO
                setting: "This request is initiated by the user."
                policy_exception_justification: "The beta update manifest is hosted by Whister."
              })"));
        url_loader_->DownloadToString(
            web_ui()
                ->GetWebContents()
                ->GetBrowserContext()
                ->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess()
                .get(),
            base::BindOnce(&WhisterSettingsHandler::OnUpdateManifestDownloaded,
                           base::Unretained(this)),
            64 * 1024);
    }

    void WhisterSettingsHandler::OnUpdateManifestDownloaded(
        std::optional<std::string> response_body) {
        if (!response_body) {
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateCheckResult",
                base::Value("error"), base::Value(""), base::Value(""),
                base::Value(""), base::Value(0));
            return;
        }

        std::optional<base::Value> parsed =
            base::JSONReader::Read(*response_body, 0);
        const base::DictValue* manifest =
            parsed ? parsed->GetIfDict() : nullptr;
        const base::DictValue* installer =
            manifest ? manifest->FindDict("installer") : nullptr;
        const std::string* channel =
            manifest ? manifest->FindString("channel") : nullptr;
        const std::string* version =
            manifest ? manifest->FindString("version") : nullptr;
        const std::string* url =
            installer ? installer->FindString("url") : nullptr;
        const std::string* sha256 =
            installer ? installer->FindString("sha256") : nullptr;
        const std::optional<int> size =
            installer ? installer->FindInt("size") : std::nullopt;
        const std::optional<int> schema_version =
            manifest ? manifest->FindInt("schemaVersion") : std::nullopt;

        GURL installer_url(url ? *url : std::string());
        const base::Version latest_version(version ? *version : std::string());
        const base::Version current_version(version_info::GetVersionNumber());
        if (!manifest || !installer || !channel || *channel != "beta" ||
            !schema_version || *schema_version != 1 || !version ||
            !latest_version.IsValid() || !current_version.IsValid() || !url ||
            !installer_url.SchemeIs("https") ||
            installer_url.host() != kUpdateHost || !sha256 ||
            !IsValidSha256(*sha256) || !size || *size <= 0 ||
            *size > kMaximumInstallerSize ||
            installer_url.path() !=
                "/win/beta/WhisterSetup-" + *version + ".exe") {
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateCheckResult",
                base::Value("error"), base::Value(""), base::Value(""),
                base::Value(""), base::Value(0));
            return;
        }

        const bool update_available =
            latest_version.CompareTo(current_version) > 0;
        if (update_available) {
            available_update_ = UpdateInfo{*version, *url,
                                           base::ToLowerASCII(*sha256), *size};
        }
        const char* status =
            update_available ? "update_available" : "up_to_date";
        web_ui()->CallJavascriptFunctionUnsafe(
            "whisterSettings.onUpdateCheckResult", base::Value(status),
            base::Value(*version), base::Value(*url),
            base::Value(base::ToLowerASCII(*sha256)), base::Value(*size));
    }

    void WhisterSettingsHandler::DownloadAndVerifyUpdate(
        const base::ListValue& args) {
        if (!available_update_) {
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateDownloadResult",
                base::Value("error"), base::Value(""), base::Value(""));
            return;
        }

        base::FilePath temp_dir;
        if (!base::GetTempDir(&temp_dir)) {
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateDownloadResult",
                base::Value("error"), base::Value(""), base::Value(""));
            return;
        }
        const base::FilePath installer_path = temp_dir.AppendASCII(
            "WhisterSetup-" + available_update_->version + ".exe");
        base::DeleteFile(installer_path);

        auto request = std::make_unique<network::ResourceRequest>();
        request->url = GURL(available_update_->url);
        request->method = "GET";
        request->credentials_mode = network::mojom::CredentialsMode::kOmit;
        request->redirect_mode = network::mojom::RedirectMode::kError;
        url_loader_ = network::SimpleURLLoader::Create(
            std::move(request),
            net::DefineNetworkTrafficAnnotation("whister_verified_installer", R"(
              semantics {
                sender: "Whister Browser Update Download"
                description: "Downloads a release selected by the user for SHA-256 verification."
                trigger: "The user selects Download and verify in Whister Settings."
                data: "No user data. The request downloads the public installer."
                destination: OTHER
              }
              policy {
                cookies_allowed: NO
                setting: "This download is initiated by the user."
                policy_exception_justification: "The beta installer is hosted by Whister."
              })"));
        url_loader_->DownloadToFile(
            web_ui()
                ->GetWebContents()
                ->GetBrowserContext()
                ->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess()
                .get(),
            base::BindOnce(&WhisterSettingsHandler::OnUpdateDownloaded,
                           base::Unretained(this), installer_path),
            installer_path, available_update_->size);
        web_ui()->CallJavascriptFunctionUnsafe(
            "whisterSettings.onUpdateDownloadResult",
            base::Value("downloading"), base::Value(""), base::Value(""));
    }

    void WhisterSettingsHandler::OnUpdateDownloaded(
        const base::FilePath& path,
        base::FilePath output_path) {
        if (!available_update_ || output_path.empty() ||
            output_path != path ||
            base::GetFileSize(path) != available_update_->size) {
            base::DeleteFile(path);
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateDownloadResult",
                base::Value("error"), base::Value(""), base::Value(""));
            return;
        }

        const std::optional<std::string> hash = HashFileSha256(path);
        if (!hash || *hash != available_update_->sha256) {
            base::DeleteFile(path);
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateDownloadResult",
                base::Value("checksum_mismatch"), base::Value(""),
                base::Value(available_update_->sha256));
            return;
        }

        verified_installer_path_ = path;
        web_ui()->CallJavascriptFunctionUnsafe(
            "whisterSettings.onUpdateDownloadResult",
            base::Value("verified"), base::Value(available_update_->version),
            base::Value(*hash));
    }

    void WhisterSettingsHandler::InstallVerifiedUpdate(
        const base::ListValue& args) {
        if (!available_update_ || verified_installer_path_.empty() ||
            base::GetFileSize(verified_installer_path_) !=
                available_update_->size) {
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateDownloadResult",
                base::Value("error"), base::Value(""), base::Value(""));
            return;
        }
        const std::optional<std::string> hash =
            HashFileSha256(verified_installer_path_);
        if (!hash || *hash != available_update_->sha256) {
            base::DeleteFile(verified_installer_path_);
            verified_installer_path_.clear();
            web_ui()->CallJavascriptFunctionUnsafe(
                "whisterSettings.onUpdateDownloadResult",
                base::Value("checksum_mismatch"), base::Value(""),
                base::Value(available_update_->sha256));
            return;
        }

        base::LaunchOptions options;
        options.start_hidden = false;
        base::CommandLine command(verified_installer_path_);
        const base::Process process = base::LaunchProcess(command, options);
        web_ui()->CallJavascriptFunctionUnsafe(
            "whisterSettings.onUpdateDownloadResult",
            base::Value(process.IsValid() ? "installer_started" : "error"),
            base::Value(available_update_->version),
            base::Value(*hash));
    }







}  // namespace whister
