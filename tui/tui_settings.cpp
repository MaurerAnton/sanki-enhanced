#include "tui_settings.h"
#include "tui_app.h"
#include "tui_utils.h"
#include "global.h"

#include <QDebug>
#include <QVariant>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

namespace t = ftxui;

TuiSettings::TuiSettings() {}

void TuiSettings::init(TuiApp* app)
{
    this->app = app;
    loadSettings();
    buildComponent();
}

void TuiSettings::loadSettings()
{
    settings = new QSettings(directories::globalSettings.fileName(), QSettings::IniFormat);
    fontPointSize = settings->value("playFontSize", 12).toInt();
    fontFamily = qstrToStd(settings->value("playFontFamily", "monospace").toString());
    tapGesture = settings->value("tapGesture").toBool();
    refreshCard = settings->value("refreshCard", 10).toInt();
}

void TuiSettings::saveSettings()
{
    settings->setValue("playFontSize", fontPointSize);
    settings->setValue("playFontFamily", stdToQstr(fontFamily));
    settings->setValue("tapGesture", tapGesture);
    settings->setValue("refreshCard", refreshCard);
    settings->sync();
    statusText = "Settings saved.";
}

void TuiSettings::buildComponent()
{
    component = t::Renderer([this] {
        t::Elements items;
        items.push_back(t::center(t::border(t::bold(t::text(" Settings ")))));
        items.push_back(t::separator());

        items.push_back(t::hbox({
            t::bold(t::text(" Font Size: ")),
            t::dim(t::text("  < ")),
            selectedSetting == 0 ? t::inverted(t::text(" " + std::to_string(fontPointSize) + "pt "))
                                 : t::text(" " + std::to_string(fontPointSize) + "pt "),
            t::dim(t::text(" >  ")),
        }));
        items.push_back(t::hbox({
            t::bold(t::text(" Font Family: ")),
            selectedSetting == 1 ? t::inverted(t::text(" " + fontFamily + " "))
                                 : t::text(" " + fontFamily + " "),
            t::dim(t::text(" (terminal default) ")),
        }));
        items.push_back(t::separator());
        items.push_back(t::hbox({
            t::bold(t::text(" Tap Gesture: ")),
            selectedSetting == 2 ? t::inverted(t::text(tapGesture ? " Enabled " : " Disabled "))
                                 : t::text(tapGesture ? " Enabled " : " Disabled "),
        }));
        items.push_back(t::hbox({
            t::bold(t::text(" Refresh Rate: ")),
            selectedSetting == 3 ? t::inverted(t::text(" " + std::to_string(refreshCard) + " cards "))
                                 : t::text(" " + std::to_string(refreshCard) + " cards "),
            t::dim(t::text(" (for e-ink builds) ")),
        }));

        if (!statusText.empty()) {
            items.push_back(t::separator());
            items.push_back(t::center(t::color(t::Color::Yellow, t::text(" " + statusText + " "))));
        }
        items.push_back(t::separator());
        items.push_back(t::center(t::hbox({
            t::bold(t::text(" [j/k] Select  [h/l] Adjust  [s] Save  [Esc] Back ")),
        })));

        return t::size(t::WIDTH, t::GREATER_THAN, 45)(t::border(t::vbox(std::move(items))));
    });

    component |= t::CatchEvent([this](t::Event e) -> bool {
        if (e == t::Event::Escape) { delete settings; settings = nullptr; app->navigateTo(TuiScreen::MENU); return true; }
        if (e == t::Event::Character('j')) { if (selectedSetting > 0) selectedSetting--; return true; }
        if (e == t::Event::Character('k')) { if (selectedSetting < 3) selectedSetting++; return true; }
        if (e == t::Event::ArrowLeft || e == t::Event::Character('h')) {
            switch (selectedSetting) {
            case 0: if (fontPointSize > 6) fontPointSize--; break;
            case 2: tapGesture = !tapGesture; break;
            case 3: if (refreshCard > 1) refreshCard--; break;
            }
            saveSettings(); return true;
        }
        if (e == t::Event::ArrowRight || e == t::Event::Character('l')) {
            switch (selectedSetting) {
            case 0: if (fontPointSize < 48) fontPointSize++; break;
            case 2: tapGesture = !tapGesture; break;
            case 3: if (refreshCard < 100) refreshCard++; break;
            }
            saveSettings(); return true;
        }
        if (e == t::Event::Character('s') || e == t::Event::Character('S')) { saveSettings(); return true; }
        return false;
    });
}

t::Component TuiSettings::getComponent() { return component; }
