#include "chrome/browser/ui/webui/whister/whister_settings_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/whister/whister_settings_handler.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace whister {

namespace {

void AddSource(content::WebUI* web_ui, const char* host, int html_id) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui), host);
  source->SetDefaultResource(html_id);
  source->AddResourcePath("settings.css", IDR_WHISTER_SETTINGS_CSS);
  source->AddResourcePath("settings.js", IDR_WHISTER_SETTINGS_JS);
  web_ui->AddMessageHandler(std::make_unique<WhisterSettingsHandler>());
}

void AddNewTabSource(content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), "whister-newtab");
  source->SetDefaultResource(IDR_WHISTER_NEW_TAB_HTML);
  source->AddResourcePath("style.css", IDR_WHISTER_NEW_TAB_CSS);
  source->AddResourcePath("app.js", IDR_WHISTER_NEW_TAB_JS);
  source->AddResourcePath("whister-logo.png", IDR_WHISTER_NEW_TAB_LOGO);
  web_ui->AddMessageHandler(std::make_unique<WhisterSettingsHandler>());
}

}  // namespace

WhisterSettingsUI::WhisterSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  AddSource(web_ui, "whister-settings", IDR_WHISTER_SETTINGS_HTML);
}

WhisterSettingsUI::~WhisterSettingsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WhisterSettingsUI)

WhisterSetupUI::WhisterSetupUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  AddSource(web_ui, "whister-setup", IDR_WHISTER_SETUP_HTML);
}

WhisterSetupUI::~WhisterSetupUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WhisterSetupUI)

WhisterNewTabUI::WhisterNewTabUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  AddNewTabSource(web_ui);
}

WhisterNewTabUI::~WhisterNewTabUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WhisterNewTabUI)

}  // namespace whister
