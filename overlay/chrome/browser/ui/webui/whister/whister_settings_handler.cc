#include "chrome/browser/ui/webui/whister/whister_settings_handler.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "components/prefs/pref_service.h"
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

namespace whister {

namespace {

enum class Type { kBool, kString, kInt };

struct Setting {
  const char* key;
  Type type;
  int min = 0;
  int max = 0;
};

// Only prefs listed here can be touched from the page.
constexpr Setting kSettings[] = {
    {"browser.show_home_button", Type::kBool},
    {"homepage_is_newtabpage", Type::kBool},
    {"homepage", Type::kString},
    {"session.restore_on_startup", Type::kInt, 1, 5},
    {"enable_do_not_track", Type::kBool},
    {"search.suggest_enabled", Type::kBool},
    {"alternate_error_pages.enabled", Type::kBool},
    {"download.prompt_for_download", Type::kBool},
    {"download.open_pdf_in_system_reader", Type::kBool},
    {"browser.theme.color_scheme2", Type::kInt, 0, 2},
    {"bookmark_bar.show_on_all_tabs", Type::kBool},
    {"webkit.webprefs.default_font_size", Type::kInt, 9, 40},
    {"webkit.webprefs.default_fixed_font_size", Type::kInt, 9, 40},
    {"webkit.webprefs.minimum_font_size", Type::kInt, 0, 24},
    {"browser.enable_spellchecking", Type::kBool},
    {"translate.enabled", Type::kBool},
    {"autofill.profile_enabled", Type::kBool},
    {"autofill.credit_card_enabled", Type::kBool},
    {"credentials_enable_service", Type::kBool},
    {"safebrowsing.enabled", Type::kBool},
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
      "installUblockLite",
      base::BindRepeating(&WhisterSettingsHandler::HandleInstallUblockLite,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "navigateToUrl",
      base::BindRepeating(&WhisterSettingsHandler::HandleNavigateToUrl,
                          base::Unretained(this)));
}

void WhisterSettingsHandler::HandleNavigateToUrl(
    const base::ListValue& args) {
  if (args.size() != 1 || !args[0].is_string() ||
      args[0].GetString().size() > 4096) {
    return;
  }
  GURL url(args[0].GetString());
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  web_ui()->GetWebContents()->OpenURL(
      content::OpenURLParams::CreateBrowserInitiated(
          url, WindowOpenDisposition::CURRENT_TAB,
          ui::PAGE_TRANSITION_TYPED),
      /*navigation_handle_callback=*/{});
}

void WhisterSettingsHandler::HandleInstallUblockLite(
    const base::ListValue& args) {
  constexpr char kExtensionId[] = "ddkjiahejlhfcafbddmgiahcphecmpfh";
  constexpr char kUpdateUrl[] =
      "https://clients2.google.com/service/update2/crx";
  bool request_started = false;

  if (args.size() == 1 && args[0].is_bool() && args[0].GetBool()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    auto* provider_manager =
        extensions::ExternalProviderManager::Get(profile);
    auto* updater = extensions::ExtensionUpdater::Get(profile);
    if (provider_manager && updater && updater->enabled()) {
      extensions::ExternalInstallInfoUpdateUrl install_info(
          kExtensionId, std::string(), GURL(kUpdateUrl),
          extensions::mojom::ManifestLocation::kExternalPrefDownload,
          extensions::Extension::NO_FLAGS,
          /*mark_acknowledged=*/false);
      request_started = provider_manager->OnExternalExtensionUpdateUrlFound(
          install_info, /*force_update=*/false);
      if (request_started) {
        extensions::ExtensionUpdater::CheckParams params;
        params.ids.push_back(kExtensionId);
        params.install_immediately = true;
        updater->CheckNow(std::move(params));
      }
    }
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "whisterSettings.onUblockInstallRequested", request_started);
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
    TemplateURL* engine = list_controller_->GetTemplateURL(args[0].GetInt());
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
    entry.Set("id", static_cast<int>(engine->id()));
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

}  // namespace whister
