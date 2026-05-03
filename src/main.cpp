#include <stdio.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Game.h"
#include <filesystem>

// --- App Constants ---
constexpr float BASE_FONT_SIZE = 22.0f;
constexpr float BG_COLOR_R = 0.2f;
constexpr float BG_COLOR_G = 0.4f;
constexpr float BG_COLOR_B = 0.2f;
constexpr float BG_COLOR_A = 1.0f;

// Error callback for GLFW
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
    // Ensure the application always looks for assets relative to the executable
    if (argc > 0) {
        std::filesystem::path exePath = std::filesystem::absolute(argv[0]).parent_path();
        std::filesystem::current_path(exePath);
    }

    glfwSetErrorCallback(glfw_error_callback);
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    if (!glfwInit())
        return 1;

    float dpi_scale = 1.0f;
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
    float dummy_y = 1.0f;
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &dpi_scale, &dummy_y);
#endif
#ifdef __APPLE__
    dpi_scale = 1.0f; // macOS coordinates are logical, no manual size multiplication required
#endif

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow((int)(Game::REFERENCE_WINDOW_WIDTH * dpi_scale), (int)(Game::REFERENCE_WINDOW_HEIGHT * dpi_scale), "ImGui Solitaire", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup high-res scalable vector font (Available in ImGui 1.92+)
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = BASE_FONT_SIZE;
    io.Fonts->AddFontDefaultVector();

    // Setup High-DPI scaling
    style.ScaleAllSizes(dpi_scale);
    io.ConfigDpiScaleFonts = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    Game game;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create a full screen window for the game
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Solitaire Board", nullptr, window_flags);
        ImGui::PopStyleVar();

        game.UpdateAndDraw();

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        glClearColor(BG_COLOR_R, BG_COLOR_G, BG_COLOR_B, BG_COLOR_A); // Green felt color
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
