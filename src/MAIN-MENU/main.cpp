/*
    Author: Liam Nayyar
    Purpose:
    Main entry point for the Aetherion launcher.
    This is just the main cpp file for the menu. Nothing special.
*/

#include "libraries.hpp"
#include "launcher.hpp"

namespace ui { // yipeee 219 calls for imgui alone!!!!!!!!!!!!
    enum class LaunchMode {
        None,
        Sim2D,
        Sim3D
    };

    enum class Tab {
        Overview,
        Simulation,
        DataAnalysis,
        ObjectLibrary,
        Export,
        Settings
    };

    struct AppState {
        LaunchMode launchMode = LaunchMode::None;
        bool wantsExit = false;
        std::string statusText = "ready";
        int sessionState = 0; // 0 = no session, 1 = session loaded
        int modelState = 2; // 0 = complete, 1 = pending, 2 = pending
        Tab currentTab = Tab::Overview;
        char loadPath[256] = "";
        char exportFolder[256] = "saves/";
        int exportTypeIndex = 0;
        char customBHName[64] = "Custom Black Hole";
        float customBHRadius = 1.0f;
        bool customIs3D = false;
        char keybindMove[32] = "WASD";
        char keybindTogglePreset[32] = "T";
        char keybindExport[32] = "X";
        bool showTooltips = true;
    };

    void applyTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.GrabRounding = 4.0f;
        
        // Professional dark theme - similar to the reference
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.11f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.16f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.22f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.25f, 1.0f);
        
        // Button styling
        style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.35f, 0.60f, 0.85f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.45f, 0.80f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.30f, 0.55f, 1.0f);
        
        // Text colors
        style.Colors[ImGuiCol_Text] = ImVec4(0.93f, 0.93f, 0.96f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.65f, 1.0f);
        
        // Separator
        style.Colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.35f, 0.5f);
        
        // Padding & spacing
        style.ItemSpacing = ImVec2(8.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.WindowPadding = ImVec2(0.0f, 0.0f);
    }

    bool sidebarButton(const char* label, Tab tab, AppState& state) {
        bool selected = state.currentTab == tab;
        if (ImGui::Selectable(label, selected, 0, ImVec2(0, 20))) {
            state.currentTab = tab;
        }
        return selected;
    }

    void drawSidebar(AppState& state) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(280, ImGui::GetIO().DisplaySize.y));
        ImGui::Begin("##Sidebar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 1.0f));
        ImGui::Spacing();
        ImGui::SetCursorPosX(20);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Aetherion");
        ImGui::SetCursorPosX(20);
        ImGui::TextDisabled("ASTROPHYSICS SUITE");
        ImGui::SetCursorPosX(20);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(20);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "WORKSPACE");
        ImGui::SetCursorPosX(30);
        sidebarButton("Overview", Tab::Overview, state);
        ImGui::SetCursorPosX(30);
        sidebarButton("Simulation", Tab::Simulation, state);
        ImGui::SetCursorPosX(30);
        sidebarButton("Data Analysis", Tab::DataAnalysis, state);
        ImGui::SetCursorPosX(30);
        sidebarButton("Object Library", Tab::ObjectLibrary, state);
        ImGui::Spacing();
        ImGui::SetCursorPosX(20);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetCursorPosX(20);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "TOOLS");
        ImGui::SetCursorPosX(30);
        sidebarButton("Export", Tab::Export, state);
        ImGui::SetCursorPosX(30);
        sidebarButton("Settings", Tab::Settings, state);
        ImGui::SetCursorPos(ImVec2(20, ImGui::GetWindowHeight() - 60));
        ImGui::SetCursorPosX(20);
        ImGui::Separator();
        ImGui::SetCursorPosX(20);
        ImGui::TextDisabled("v0.0.1 • open source");
        ImGui::PopStyleColor();
        ImGui::End();
    }

    void drawModeCard(const char* modeNum, const char* title, const char* description,
                      LaunchMode mode, AppState& state) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.12f, 0.18f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.45f, 0.75f, 0.6f));
        
        ImGui::BeginChild(title, ImVec2((avail.x - 20) * 0.45f, 180), true);
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", modeNum);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "%s", title);
        ImGui::Spacing();
        ImGui::PushTextWrapPos();
        ImGui::TextDisabled("%s", description);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button("Launch"))
            state.launchMode = mode;
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    void drawOverviewTab(AppState& state) {
        ImGui::SetCursorPosX(30);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Overview");
        ImGui::TextDisabled("Start here, browse recent progress, or pick a mode.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "Select Mode");
        ImGui::Spacing();
        drawModeCard("MODE 01", "2D Research Mode",
            "Advanced calculations, data compilation, scientific analysis, BLR simulation",
            LaunchMode::Sim2D, state);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
        drawModeCard("MODE 02", "3D Visualization Mode",
            "Real-time rendering, visual exploration, gravitational lensing preview",
            LaunchMode::Sim3D, state);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "Recently used");
        ImGui::TextDisabled("Resume progress files from your last session.");
        ImGui::Spacing();
        const char* recentFiles[] = {"progress-2026-04-10.ahp", "workspace-sgrA-2d.ahp", "lens-study-3d.ahp"};                                                                 

        for (int index = 0; index < 3; ++i) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.15f, 0.22f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.25f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.20f, 0.30f, 1.0f));
            if (ImGui::Button(recentFiles[i], ImVec2(-1.0f, 0))) {
                state.statusText = std::string("Restored ") + recentFiles[i];
                state.sessionState = 1;
            }
            ImGui::PopStyleColor(3);
        }
    }

    void drawSimulationTab(AppState& state) {
        ImGui::SetCursorPosX(30);                                               
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Simulation");    // send this 2-3 dagestan and forget
        ImGui::TextDisabled("Load a saved simulation or start a new 2D/3D run.");
        ImGui::Spacing();
        ImGui::Separator(); 
        ImGui::Spacing();
        ImGui::InputText("Simulation file path", state.loadPath, sizeof(state.loadPath));
        if (ImGui::Button("Load simulation")) {
            state.statusText = state.loadPath[0] ? std::string("Loaded ") + state.loadPath : "No file selected";
            if (state.loadPath[0]) state.sessionState = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("New 2D simulation"))
            state.launchMode = LaunchMode::Sim2D;
        ImGui::SameLine();
        if (ImGui::Button("New 3D simulation"))
            state.launchMode = LaunchMode::Sim3D;
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "Preset previews");
        ImGui::Spacing();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const struct PreviewEntry { const char* title; const char* desc; ImVec4 color; } previews[] = {
            {"TON 618", "Biggest confirmed quasar", ImVec4(0.65f, 0.40f, 0.95f, 0.45f)},
            {"Sgr A*", "Our local quasar", ImVec4(0.25f, 0.70f, 0.95f, 0.45f)},
            {"Custom BH", "Make your own black hole and galaxy", ImVec4(0.95f, 0.55f, 0.30f, 0.45f)}
        };
        for (int index = 0; index < 3; ++i) {
            ImGui::PushStyleColor(ImGuiCol_Button, previews[i].color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(previews[i].color.x + 0.1f, previews[i].color.y + 0.1f, previews[i].color.z + 0.1f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(previews[i].color.x + 0.05f, previews[i].color.y + 0.05f, previews[i].color.z + 0.05f, 0.7f));
            ImGui::BeginChild(previews[i].title, ImVec2((avail.x - 20.0f) / 3.0f, 140), true);
            ImGui::PushTextWrapPos();
            ImGui::TextColored(ImVec4(0.98f, 0.98f, 0.98f, 1.0f), "%s", previews[i].title);
            ImGui::TextDisabled("%s", previews[i].desc);
            ImGui::Spacing();
            if (ImGui::Button("Select preset", ImVec2(-1.0f, 0))) {
                state.statusText = std::string("Preset selected: ") + previews[i].title;
                state.sessionState = 1;
            }
            ImGui::PopTextWrapPos();
            ImGui::EndChild();
            ImGui::PopStyleColor(3);
            if (index < 2) ImGui::SameLine();
        }
        ImGui::Spacing(); // dear lord the amount of functions we're invoking....
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "Custom black hole");
        ImGui::InputText("Name", state.customBHName, sizeof(state.customBHName));
        ImGui::Checkbox("3D black hole", &state.customIs3D);
        ImGui::SliderFloat("Schwarzschild radius", &state.customBHRadius, 0.1f, 50.0f, "%.2f Rs");
        if (ImGui::Button("Create custom black hole")) {
            state.statusText = std::string("Created ") + state.customBHName;
            state.sessionState = 1;
        }
    }

    void drawDataAnalysisTab(AppState& state) {
        ImGui::SetCursorPosX(30);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Data Analysis");
        ImGui::TextDisabled("Coming soon — this space will host analysis controls and charts.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Dummy(ImVec2(0, 340));
        ImGui::TextDisabled("Blank workspace.");
    }

    void drawObjectLibraryTab(AppState& state) {
        ImGui::SetCursorPosX(30);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Object Library");
        ImGui::TextDisabled("Browse custom presets for black holes and simulation objects.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        const struct LibraryEntry { const char* title; const char* subtitle; const char* tag; ImVec4 color; } entries[] = {
            {"2D Micro BH", "Thin-disk research preset", "Black Hole", ImVec4(0.40f, 0.70f, 0.95f, 1.0f)},
            {"3D Rapid Spin", "High-magnification disk", "Black Hole", ImVec4(0.80f, 0.45f, 0.95f, 1.0f)},
            {"Pulsar Beacon", "Fast-rotating neutron star", "Object", ImVec4(0.95f, 0.55f, 0.30f, 1.0f)},
            {"Nebula Cloud", "Diffuse gas cluster", "Object", ImVec4(0.30f, 0.85f, 0.55f, 1.0f)},
            {"Asteroid Swarm", "Infalling debris field", "Object", ImVec4(0.95f, 0.75f, 0.35f, 1.0f)}
        };
        for (int index = 0; index < 5; ++i) {
            ImGui::PushStyleColor(ImGuiCol_Text, entries[i].color);
            ImGui::Text("%s", entries[i].title);
            ImGui::PopStyleColor();
            ImGui::TextDisabled("%s • %s", entries[i].subtitle, entries[i].tag);
            ImGui::Spacing();
        }
    }

    void drawExportTab(AppState& state) {
        ImGui::SetCursorPosX(30);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Export");
        ImGui::TextDisabled("Choose export folder and file type for CSV/FITS output.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputText("Export folder", state.exportFolder, sizeof(state.exportFolder));
        const char* types[] = {"CSV", "FITS", "Binary"};
        ImGui::Combo("File type", &state.exportTypeIndex, types, IM_ARRAYSIZE(types));
        ImGui::Spacing();
        ImGui::TextDisabled("All exported data will be stored under the selected folder by default.");
        ImGui::Spacing();
        if (ImGui::Button("Apply export settings")) {
            state.statusText = std::string("Export set to ") + state.exportFolder + " (" + types[state.exportTypeIndex] + ")";
        }
    }
    /*--- Settings tab is last since it has more complex state management ---*/
    void drawSettingsTab(AppState& state) {
        ImGui::SetCursorPosX(30);
        ImGui::TextColored(ImVec4(0.93f, 0.93f, 0.96f, 1.0f), "Settings");
        ImGui::TextDisabled("Modify keybinds and common interface options.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputText("Move camera", state.keybindMove, sizeof(state.keybindMove));
        ImGui::InputText("Toggle preset", state.keybindTogglePreset, sizeof(state.keybindTogglePreset));
        ImGui::InputText("Export quick key", state.keybindExport, sizeof(state.keybindExport));
        ImGui::Checkbox("Show tooltips", &state.showTooltips);
        ImGui::Spacing();
        if (ImGui::Button("Save settings")) {
            state.statusText = "Settings updated";
        }
    }

    void drawMainContent(AppState& state) {
        ImGui::SetNextWindowPos(ImVec2(280, 0));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 280, ImGui::GetIO().DisplaySize.y));
        ImGui::Begin("##MainContent", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        switch (state.currentTab) {
            case Tab::Overview: drawOverviewTab(state); break;
            case Tab::Simulation: drawSimulationTab(state); break;
            case Tab::DataAnalysis: drawDataAnalysisTab(state); break;
            case Tab::ObjectLibrary: drawObjectLibraryTab(state); break;
            case Tab::Export: drawExportTab(state); break;
            case Tab::Settings: drawSettingsTab(state); break;
        }
        ImGui::End();
    }
    /*--------- Status bar ---------*/
    void drawStatusBar(AppState& state) {
        ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 30));
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 30));
        ImGui::Begin("##StatusBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar);
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.09f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
        
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.30f, 1.0f), u8"● %s", state.statusText.c_str());
        
        ImGui::SameLine(150);
        if (state.sessionState == 1) {
            ImGui::TextDisabled("| session loaded");
        } else {
            ImGui::TextDisabled("| no session loaded");
        }
        
        ImGui::SameLine(300);
        const char* modelStr = state.modelState == 0 ? "complete" : "pending";
        ImGui::TextDisabled("| blueshift model - %s", modelStr);
        
        ImGui::PopStyleColor(2);
        ImGui::End();
    }

    void drawUI(AppState& state) {
        drawSidebar(state);
        drawMainContent(state);
        drawStatusBar(state);
    }
}

/*--------- Entry point ---------*/
int main() {
    constexpr unsigned int WIN_W = 1200;
    constexpr unsigned int WIN_H = 800;

    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u(WIN_W, WIN_H)),
        "Aetherion Astrophysics Suite",
        sf::Style::Close);
    window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Error: Failed to initialize ImGui-SFML.\n";
        exit(1); 
    }

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;

    sf::Clock deltaClock;

    /*--------------------- Main program loop ---------------------*/
    while (window.isOpen()) {
        while (auto evOpt = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *evOpt);
            if (evOpt->is<sf::Event::Closed>())
                window.close();
        }

        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(window, dt);

        /*--------------------- In-window IMGUI content ---------------------*/
        static ui::AppState state;
        ui::applyTheme();

        ui::drawUI(state);

        /*--------- Handle launch request ---------*/
        if (state.launchMode != ui::LaunchMode::None) {
            std::string exec;
            
            if (state.launchMode == ui::LaunchMode::Sim2D)
                exec = "blackhole-2D";
            else if (state.launchMode == ui::LaunchMode::Sim3D)
                exec = "blackhole-3D";
            
            if (!exec.empty()) {
                bool ok = launcher::launchSimulator(exec);
                if (!ok) {
                    state.statusText = "launch failed";
                }
            }
            
            state.launchMode = ui::LaunchMode::None;
        }

        /*--------- Handle exit ---------*/
        if (state.wantsExit)
            window.close();

        /*--------- Render ---------*/
        window.clear(sf::Color(13, 13, 18));
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}