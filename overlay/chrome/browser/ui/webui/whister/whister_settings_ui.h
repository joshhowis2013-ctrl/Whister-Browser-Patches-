#ifndef CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace whister {

class WhisterSettingsUI;
class WhisterSetupUI;
class WhisterNewTabUI;

class WhisterNewTabUIConfig
  : public content::DefaultWebUIConfig<WhisterNewTabUI> {
 public:
  WhisterNewTabUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, "whister-newtab") {}
};

// chrome://whister-settings
class WhisterSettingsUIConfig
    : public content::DefaultWebUIConfig<WhisterSettingsUI> {
 public:
  WhisterSettingsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, "whister-settings") {}
};

// chrome://whister-setup, shown on first run in place of chrome://welcome.
class WhisterSetupUIConfig : public content::DefaultWebUIConfig<WhisterSetupUI> {
 public:
  WhisterSetupUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, "whister-setup") {}
};

class WhisterSettingsUI : public content::WebUIController {
 public:
  explicit WhisterSettingsUI(content::WebUI* web_ui);
  ~WhisterSettingsUI() override;

  WhisterSettingsUI(const WhisterSettingsUI&) = delete;
  WhisterSettingsUI& operator=(const WhisterSettingsUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class WhisterSetupUI : public content::WebUIController {
 public:
  explicit WhisterSetupUI(content::WebUI* web_ui);
  ~WhisterSetupUI() override;

  WhisterSetupUI(const WhisterSetupUI&) = delete;
  WhisterSetupUI& operator=(const WhisterSetupUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class WhisterNewTabUI : public content::WebUIController {
 public:
  explicit WhisterNewTabUI(content::WebUI* web_ui);
  ~WhisterNewTabUI() override;

  WhisterNewTabUI(const WhisterNewTabUI&) = delete;
  WhisterNewTabUI& operator=(const WhisterNewTabUI&) = delete;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace whister

#endif  // CHROME_BROWSER_UI_WEBUI_WHISTER_WHISTER_SETTINGS_UI_H_
