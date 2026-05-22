#ifndef TUI_SETTINGS_H
#define TUI_SETTINGS_H

#include <ftxui/component/component.hpp>
#include <QSettings>
#include <QString>

class TuiApp;

class TuiSettings {
public:
    TuiSettings();
    void init(TuiApp* app);
    ftxui::Component getComponent();

    TuiApp* app = nullptr;

private:
    void loadSettings();
    void saveSettings();
    void buildComponent();

    QSettings* settings = nullptr;
    std::string statusText;

    int fontPointSize = 12;
    std::string fontFamily = "monospace";
    bool tapGesture = false;
    int refreshCard = 10;

    ftxui::Component component;
    int selectedSetting = 0;
};

#endif // TUI_SETTINGS_H
