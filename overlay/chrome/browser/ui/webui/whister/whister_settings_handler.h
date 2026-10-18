#ifndef CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_HANDLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class KeywordEditorController;

namespace whister {

// Reads and writes an allow-listed set of profile prefs for the Whister
// settings and setup pages.
class WhisterSettingsHandler : public content::WebUIMessageHandler {
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
  void HandleInstallUblockLite(const base::ListValue& args);
  void HandleNavigateToUrl(const base::ListValue& args);
  void SendSettings();
  void SendSearchEngines();
  void TriggerServerUpdateCheck(const base::ListValue& args);
  void OnServerResponseReceived(std::optional<std::string> response_body);
  void HandleSmartDownloadExecution(const base::ListValue& args);
  void OnSmartPayloadDownloaded(const base::FilePath& target_path, base::FilePath output_file_path);



  base::CallbackListSubscription loaded_subscription_;
  std::unique_ptr<KeywordEditorController> list_controller_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace whister

#endif  // CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_HANDLER_H_
