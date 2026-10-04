#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float PI = 3.14159265358979323846f;
constexpr double STEP = 1.0 / 120.0;
constexpr float VICTORY_DURATION = 4.5f;
constexpr ImU32 PALETTE[] = {
    IM_COL32(255, 205, 100, 255), IM_COL32(255, 245, 219, 255),
    IM_COL32(245, 91, 108, 255), IM_COL32(118, 199, 237, 255)
};

float FrameTime(float dt) {
    return std::isfinite(dt) ? std::clamp(dt, 0.0f, 0.25f) : 0.0f;
}

ImU32 WithAlpha(ImU32 color, float alpha) {
    unsigned a = static_cast<unsigned>(255.0f * std::clamp(alpha, 0.0f, 1.0f));
    return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

float Smooth(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}
}

ParticleSystem::ParticleSystem(uint32_t seed) : m_random(seed) {
    m_particles.reserve(MAX_PARTICLES);
}

float ParticleSystem::Random(float min, float max) {
    return std::uniform_real_distribution<float>(min, max)(m_random);
}

ImVec2 ParticleSystem::ToLogical(ImVec2 screen, const View& view) const {
    return ImVec2((screen.x - view.origin.x) / view.scale, (screen.y - view.origin.y) / view.scale);
}

ImVec2 ParticleSystem::OnPerimeter(ImVec2 center, ImVec2 size) {
    float w = std::max(0.0f, size.x), h = std::max(0.0f, size.y);
    float distance = Random(0.0f, 2.0f * (w + h));
    if (distance < w) return ImVec2(center.x - w * 0.5f + distance, center.y - h * 0.5f);
    distance -= w;
    if (distance < h) return ImVec2(center.x + w * 0.5f, center.y - h * 0.5f + distance);
    distance -= h;
    if (distance < w) return ImVec2(center.x + w * 0.5f - distance, center.y + h * 0.5f);
    distance -= w;
    return ImVec2(center.x - w * 0.5f, center.y + h * 0.5f - distance);
}

ParticleSystem::Particle ParticleSystem::MakeParticle(ImVec2 pos, Shape shape, bool celebration) {
    Particle p{};
    p.pos = p.previousPos = pos;
    p.shape = shape;
    p.celebration = celebration;
    p.color = PALETTE[std::uniform_int_distribution<int>(0, 3)(m_random)];
    if (shape == Shape::Heart || shape == Shape::Diamond) p.color = PALETTE[2];
    p.lifetime = celebration ? Random(2.2f, 3.4f) : Random(0.4f, 0.7f);
    p.size = celebration ? Random(3.0f, 6.0f) : Random(1.5f, 3.0f);
    p.angle = Random(-PI, PI);
    p.spin = Random(-5.0f, 5.0f);
    p.phase = Random(0.0f, 2.0f * PI);
    p.gravity = celebration ? 260.0f : 180.0f;
    p.drag = celebration ? 0.45f : 2.4f;
    return p;
}

void ParticleSystem::Add(Particle particle) {
    if (m_particles.size() < MAX_PARTICLES) m_particles.push_back(particle);
}

void ParticleSystem::EmitMove(ImVec2 center, ImVec2 size, const View& view) {
    if (!(view.scale > 0.0f)) return;
    center = ToLogical(center, view);
    size = ImVec2(size.x / view.scale, size.y / view.scale);
    for (int i = 0; i < 26 && Count() < MAX_PARTICLES; ++i) {
        ImVec2 pos = OnPerimeter(center, size);
        float angle = std::atan2(pos.y - center.y, pos.x - center.x) + Random(-0.35f, 0.35f);
        float speed = Random(55.0f, 170.0f);
        Shape shape = i % 4 == 0 ? Shape::Diamond : Shape::Spark;
        Particle p = MakeParticle(pos, shape, false);
        p.velocity = ImVec2(std::cos(angle) * speed, std::sin(angle) * speed - 40.0f);
        Add(p);
    }
}

void ParticleSystem::EmitTrail(ImVec2 center, ImVec2 size, const View& view, float dt) {
    if (!(view.scale > 0.0f)) return;
    center = ToLogical(center, view);
    size = ImVec2(size.x / view.scale, size.y / view.scale);
    if (!m_hasTrailCenter) {
        m_lastTrailCenter = center;
        m_hasTrailCenter = true;
        return;
    }
    float dx = center.x - m_lastTrailCenter.x, dy = center.y - m_lastTrailCenter.y;
    if (dx * dx + dy * dy < 0.01f) return;
    m_trailBudget += FrameTime(dt) * 36.0;
    int count = static_cast<int>(m_trailBudget + 1e-6);
    m_trailBudget = std::max(0.0, m_trailBudget - count);
    for (int i = 0; i < count && Count() < MAX_PARTICLES; ++i) {
        float t = (i + 0.5f) / count;
        ImVec2 sample(m_lastTrailCenter.x + dx * t, m_lastTrailCenter.y + dy * t);
        Particle p = MakeParticle(OnPerimeter(sample, size), Shape::Spark, false);
        p.color = PALETTE[i % 2];
        p.size = Random(1.0f, 2.0f);
        p.lifetime = Random(0.22f, 0.4f);
        p.gravity = 25.0f;
        p.drag = 4.0f;
        p.velocity = ImVec2(Random(-12.0f, 12.0f), Random(-18.0f, 5.0f));
        Add(p);
    }
    m_lastTrailCenter = center;
}

void ParticleSystem::StopTrail() {
    m_hasTrailCenter = false;
    m_trailBudget = 0.0;
}

void ParticleSystem::EmitVolley(const View& view, bool left, int count) {
    float width = view.size.x / view.scale, height = view.size.y / view.scale;
    ImVec2 origin(width * (left ? 0.12f : 0.88f), height * 0.94f);
    for (int i = 0; i < count && Count() < MAX_PARTICLES; ++i) {
        Shape shape = i % 3 == 0 ? static_cast<Shape>(2 + i % 4) : Shape::Confetti;
        Particle p = MakeParticle(origin, shape, true);
        float angle = Random(-1.45f, -0.75f);
        float speed = Random(580.0f, 860.0f);
        p.velocity = ImVec2(std::cos(angle) * speed * (left ? 1.0f : -1.0f), std::sin(angle) * speed);
        Add(p);
    }
}

void ParticleSystem::StartVictory(const View& view) {
    if (!(view.scale > 0.0f)) return;
    StopTrail();
    m_victoryTime = 0.0f;
    m_nextVolley = 0.65f;
    m_leftVolley = true;
    EmitVolley(view, true, 110);
    EmitVolley(view, false, 110);
}

void ParticleSystem::Step(float dt, const View& view) {
    if (m_victoryTime >= 0.0f) {
        m_victoryTime += dt;
        if (m_victoryTime >= VICTORY_DURATION) m_victoryTime = -1.0f;
        else if (m_victoryTime >= m_nextVolley) {
            EmitVolley(view, m_leftVolley, 65);
            m_leftVolley = !m_leftVolley;
            m_nextVolley += 0.65f;
        }
    }
    for (auto& p : m_particles) {
        p.previousPos = p.pos;
        float damping = std::exp(-p.drag * dt);
        float flutter = p.celebration ? std::sin(p.age * 7.0f + p.phase) * 65.0f : 0.0f;
        p.velocity.x = p.velocity.x * damping + flutter * dt;
        p.velocity.y = p.velocity.y * damping + p.gravity * dt;
        p.pos.x += p.velocity.x * dt;
        p.pos.y += p.velocity.y * dt;
        p.angle += p.spin * dt;
        p.age += dt;
    }
    // One linear compaction, preserving draw order and avoiding repeated shifts.
    float width = view.size.x / view.scale, height = view.size.y / view.scale;
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [=](const Particle& p) {
        return p.age >= p.lifetime || p.pos.x < -64.0f || p.pos.x > width + 64.0f || p.pos.y > height + 64.0f;
    }), m_particles.end());
}

void ParticleSystem::Update(float dt, const View& view) {
    if (!(view.scale > 0.0f)) return;
    m_accumulator += FrameTime(dt);
    while (m_accumulator + 1e-9 >= STEP) {
        Step(static_cast<float>(STEP), view);
        m_accumulator -= STEP;
    }
}

void ParticleSystem::Draw(ImDrawList* drawList, const View& view) const {
    if (!drawList || !(view.scale > 0.0f)) return;
    drawList->PushClipRect(view.origin, ImVec2(view.origin.x + view.size.x, view.origin.y + view.size.y), true);
    float interpolation = static_cast<float>(std::clamp(m_accumulator / STEP, 0.0, 1.0));
    for (const auto& p : m_particles) {
        float progress = p.age / p.lifetime;
        float alpha = Smooth(p.age / 0.05f) * (1.0f - Smooth((progress - 0.55f) / 0.45f));
        ImVec2 pos(view.origin.x + (p.previousPos.x + (p.pos.x - p.previousPos.x) * interpolation) * view.scale,
                   view.origin.y + (p.previousPos.y + (p.pos.y - p.previousPos.y) * interpolation) * view.scale);
        float radius = p.size * view.scale;
        ImU32 color = WithAlpha(p.color, alpha);
        if (p.shape == Shape::Spark) {
            // Soft glow and a short velocity streak; no textures or extra shaders.
            drawList->AddCircleFilled(pos, radius * 3.0f, WithAlpha(p.color, alpha * 0.06f), 8);
            drawList->AddCircleFilled(pos, radius * 1.8f, WithAlpha(p.color, alpha * 0.16f), 8);
            ImVec2 tail(pos.x - p.velocity.x * view.scale * 0.025f, pos.y - p.velocity.y * view.scale * 0.025f);
            drawList->AddLine(tail, pos, WithAlpha(p.color, alpha * 0.45f), std::max(0.5f, radius * 0.6f));
            drawList->AddCircleFilled(pos, radius * (1.0f - progress * 0.4f), color, 8);
            continue;
        }

        // Build small shapes at the origin, then rotate and squash their vertices
        // to give each piece its own tumble without duplicating suit geometry.
        int firstVertex = drawList->VtxBuffer.Size;
        switch (p.shape) {
        case Shape::Confetti:
            drawList->AddRectFilled(ImVec2(-radius, -0.45f * radius), ImVec2(radius, 0.45f * radius), color);
            break;
        case Shape::Heart:
            drawList->AddCircleFilled(ImVec2(-0.4f * radius, -0.3f * radius), 0.55f * radius, color, 8);
            drawList->AddCircleFilled(ImVec2(0.4f * radius, -0.3f * radius), 0.55f * radius, color, 8);
            drawList->AddTriangleFilled(ImVec2(-0.88f * radius, -0.05f * radius), ImVec2(0.88f * radius, -0.05f * radius), ImVec2(0, radius), color);
            break;
        case Shape::Diamond:
            drawList->AddQuadFilled(ImVec2(0, -radius), ImVec2(0.7f * radius, 0), ImVec2(0, radius), ImVec2(-0.7f * radius, 0), color);
            break;
        case Shape::Club:
            drawList->AddCircleFilled(ImVec2(0, -0.45f * radius), 0.5f * radius, color, 8);
            drawList->AddCircleFilled(ImVec2(-0.45f * radius, 0.15f * radius), 0.5f * radius, color, 8);
            drawList->AddCircleFilled(ImVec2(0.45f * radius, 0.15f * radius), 0.5f * radius, color, 8);
            drawList->AddTriangleFilled(ImVec2(0, 0.1f * radius), ImVec2(-0.4f * radius, radius), ImVec2(0.4f * radius, radius), color);
            break;
        case Shape::Spade:
            drawList->AddCircleFilled(ImVec2(-0.4f * radius, 0.15f * radius), 0.5f * radius, color, 8);
            drawList->AddCircleFilled(ImVec2(0.4f * radius, 0.15f * radius), 0.5f * radius, color, 8);
            drawList->AddTriangleFilled(ImVec2(-0.85f * radius, 0), ImVec2(0.85f * radius, 0), ImVec2(0, -radius), color);
            drawList->AddTriangleFilled(ImVec2(0, 0.1f * radius), ImVec2(-0.4f * radius, radius), ImVec2(0.4f * radius, radius), color);
            break;
        case Shape::Spark: break;
        }
        float squash = p.celebration ? 0.2f + 0.8f * std::abs(std::cos(p.age * 5.0f + p.phase)) : 1.0f;
        float c = std::cos(p.angle), s = std::sin(p.angle);
        for (int i = firstVertex; i < drawList->VtxBuffer.Size; ++i) {
            ImVec2 local = drawList->VtxBuffer[i].pos;
            local.x *= squash;
            drawList->VtxBuffer[i].pos = ImVec2(pos.x + c * local.x - s * local.y, pos.y + s * local.x + c * local.y);
        }
    }
    drawList->PopClipRect();
}

void ParticleSystem::Clear() {
    m_particles.clear();
    m_accumulator = 0.0;
    m_victoryTime = -1.0f;
    m_nextVolley = 0.0f;
    StopTrail();
}
