#ifndef CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_HANDLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

class KeywordEditorController;

namespace whister {

// Reads and writes an allow-listed set of profile prefs for the Whister
// settings and setup pages.
class WhisterSettingsHandler : public content::WebUIMessageHandler,
                               public bookmarks::BookmarkModelObserver {
 public:
  WhisterSettingsHandler();
  ~WhisterSettingsHandler() override;

  WhisterSettingsHandler(const WhisterSettingsHandler&) = delete;
  WhisterSettingsHandler& operator=(const WhisterSettingsHandler&) = delete;

  void RegisterMessages() override;

 private:
  void HandleGetSettings(const base::ListValue& args);
  void HandleSetSetting(const base::ListValue& args);
  void HandleGetSearchEngines(const base::ListValue& args);
  void HandleGetAboutInfo(const base::ListValue& args);
  void HandleSetSearchEngine(const base::ListValue& args);
  void HandleNavigateToUrl(const base::ListValue& args);
  void HandleGetBookmarks(const base::ListValue& args);
  void SendBookmarks();
  void CheckForUpdates(const base::ListValue& args);
  void OnUpdateManifestDownloaded(
      std::optional<std::string> response_body);
  void DownloadAndVerifyUpdate(const base::ListValue& args);
  void OnUpdateDownloaded(const base::FilePath& path,
                          base::FilePath output_path);
  void InstallVerifiedUpdate(const base::ListValue& args);
  void OnJavascriptDisallowed() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override {}
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override {}
  void BookmarkNodeRemoved(
      const bookmarks::BookmarkNode* parent,
      size_t old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked,
      const base::Location& location) override {}
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(
      const std::set<GURL>& removed_urls,
      const base::Location& location) override {}
  void SendSettings();
  void SendSearchEngines();

  struct UpdateInfo {
    std::string version;
    std::string url;
    std::string sha256;
    int64_t size = 0;
  };

  base::CallbackListSubscription loaded_subscription_;
  std::unique_ptr<KeywordEditorController> list_controller_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_model_observation_{this};
  bool bookmarks_request_pending_ = false;
  std::optional<UpdateInfo> available_update_;
  base::FilePath verified_installer_path_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace whister

#endif  // CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_HANDLER_H_
