#include "ParticleSystem.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct ParticleTestAccess {
    static constexpr ParticleSystem::View VIEW{{0, 0}, {1280, 720}, 1.0f};

    static void TestFrameRates() {
        ParticleSystem reference(42);
        reference.StartVictory(VIEW);
        for (int i = 0; i < 60; ++i) reference.Update(1.0f / 60, VIEW);
        for (int fps : {30, 144, 240}) {
            ParticleSystem system(42);
            system.StartVictory(VIEW);
            for (int i = 0; i < fps; ++i) system.Update(1.0f / fps, VIEW);
            Require(system.Count() == reference.Count(), "victory emission depends on frame rate");
            for (size_t i = 0; i < system.Count(); ++i) {
                const auto& a = system.m_particles[i];
                const auto& b = reference.m_particles[i];
                Require(std::abs(a.pos.x - b.pos.x) < 0.01f && std::abs(a.pos.y - b.pos.y) < 0.01f,
                    "particle physics depends on frame rate");
            }
        }

        for (int fps : {30, 60, 144}) {
            ParticleSystem system(5);
            system.EmitTrail({100, 300}, {100, 140}, VIEW, 0);
            for (int i = 1; i <= fps; ++i) {
                system.EmitTrail({100 + 600.0f * i / fps, 300}, {100, 140}, VIEW, 1.0f / fps);
            }
            Require(system.Count() == 36, "drag trail emission depends on frame rate");
            auto count = system.Count();
            system.EmitTrail({700, 300}, {100, 140}, VIEW, 0.1f);
            Require(system.Count() == count, "stationary drag emits particles");
            system.StopTrail();
            system.EmitTrail({900, 300}, {100, 140}, VIEW, 0.1f);
            Require(system.Count() == count, "new drag leaves a trail from the previous drag");
        }
    }

    static void TestLimitsAndLifecycle() {
        ParticleSystem system(42);
        for (int i = 0; i < 200; ++i) system.EmitMove({640, 360}, {100, 140}, VIEW);
        Require(system.Count() == ParticleSystem::MAX_PARTICLES, "particle budget exceeded");
        auto capacity = system.m_particles.capacity();
        system.Clear();
        Require(system.Count() == 0 && system.m_particles.capacity() == capacity, "clear discards pool capacity");

        system.StartVictory(VIEW);
        for (int i = 0; i < 1200; ++i) system.Update(1.0f / 60, VIEW);
        Require(system.Count() == 0, "celebration continues emitting indefinitely");
        Require(system.m_victoryTime < 0, "celebration did not finish");
        system.StartVictory(VIEW);
        system.Clear();
        system.Update(0.25f, VIEW);
        Require(system.Count() == 0, "cleared celebration resumed emission");

        system.EmitMove({640, 360}, {100, 140}, VIEW);
        auto initialPos = system.m_particles[0].pos;
        system.Update(-1, VIEW);
        system.Update(std::numeric_limits<float>::quiet_NaN(), VIEW);
        Require(system.m_particles[0].pos.x == initialPos.x, "invalid dt advanced simulation");
        system.Update(10, VIEW);
        for (const auto& p : system.m_particles) {
            Require(std::isfinite(p.pos.x) && std::isfinite(p.pos.y) && p.age <= 0.251f,
                "frame hitch destabilized particles");
        }
    }

    static void TestScalingAndDraw() {
        ParticleSystem small(42), large(42);
        ParticleSystem::View scaled{{30, 40}, {2560, 1440}, 2};
        small.EmitMove({400, 300}, {100, 140}, VIEW);
        large.EmitMove({830, 640}, {200, 280}, scaled);
        for (int i = 0; i < 12; ++i) {
            small.Update(1.0f / 60, VIEW);
            large.Update(1.0f / 60, scaled);
        }
        Require(small.Count() == large.Count(), "DPI changes particle count");
        for (size_t i = 0; i < small.Count(); ++i) {
            Require(small.m_particles[i].pos.x == large.m_particles[i].pos.x &&
                small.m_particles[i].pos.y == large.m_particles[i].pos.y,
                "DPI changes particle physics");
        }

        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = scaled.size;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ImGui::NewFrame();
        auto* drawList = ImGui::GetForegroundDrawList();
        small.Draw(drawList, VIEW);
        large.Draw(drawList, scaled);
        // Exercise every confetti/suit shape, and partial-step interpolation.
        small.StartVictory(VIEW);
        small.Update(0.1f, VIEW);
        small.Update(0.002f, VIEW);
        small.Draw(drawList, VIEW);
        Require(drawList->VtxBuffer.Size > 0, "particles produced no geometry");
        for (const auto& vertex : drawList->VtxBuffer) {
            Require(std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y), "non-finite particle vertex");
        }
        ImGui::EndFrame();
        ImGui::DestroyContext();
    }
};

int main() {
    try {
        ParticleTestAccess::TestFrameRates();
        ParticleTestAccess::TestLimitsAndLifecycle();
        ParticleTestAccess::TestScalingAndDraw();
        std::cout << "Particle regressions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
