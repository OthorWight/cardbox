#pragma once

#include "imgui.h"
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

// Positions and physics live in logical window coordinates. Only emission and
// drawing convert to screen coordinates, keeping effects consistent at any DPI.
class ParticleSystem {
public:
    struct View {
        ImVec2 origin;
        ImVec2 size;
        float scale;
    };

    static constexpr size_t MAX_PARTICLES = 1024;
    explicit ParticleSystem(uint32_t seed = std::random_device{}());
    void EmitMove(ImVec2 center, ImVec2 size, const View& view);
    void EmitTrail(ImVec2 center, ImVec2 size, const View& view, float dt);
    void StopTrail();
    void StartVictory(const View& view);
    void Update(float dt, const View& view);
    void Draw(ImDrawList* drawList, const View& view) const;
    void Clear();
    size_t Count() const { return m_particles.size(); }

private:
    friend struct ParticleTestAccess;
    enum class Shape { Spark, Confetti, Heart, Diamond, Club, Spade };
    struct Particle {
        ImVec2 pos;
        ImVec2 previousPos;
        ImVec2 velocity;
        ImU32 color;
        Shape shape;
        float age = 0.0f;
        float lifetime;
        float size;
        float angle;
        float spin;
        float phase;
        float gravity;
        float drag;
        bool celebration = false;
    };

    std::vector<Particle> m_particles;
    std::mt19937 m_random;
    double m_accumulator = 0.0;
    double m_trailBudget = 0.0;
    ImVec2 m_lastTrailCenter;
    bool m_hasTrailCenter = false;
    float m_victoryTime = -1.0f;
    float m_nextVolley = 0.0f;
    bool m_leftVolley = true;

    float Random(float min, float max);
    ImVec2 ToLogical(ImVec2 screen, const View& view) const;
    ImVec2 OnPerimeter(ImVec2 center, ImVec2 size);
    Particle MakeParticle(ImVec2 pos, Shape shape, bool celebration);
    void Add(Particle particle);
    void EmitVolley(const View& view, bool left, int count);
    void Step(float dt, const View& view);
};
