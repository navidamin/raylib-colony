#include "survey_console.h"

#include <algorithm>
#include <cmath>

SurveyConsole::SurveyConsole()
{
    Rebuild();
}

void SurveyConsole::Rebuild()
{
    ground.Build(knowledge, scours);
    builtKnowledgeRevision = knowledge.Revision();
    builtScourRevision = scourRevision;
}

void SurveyConsole::Step(float dt)
{
    block.Step(dt);
    if (knowledge.Revision() != builtKnowledgeRevision || scourRevision != builtScourRevision)
    {
        Rebuild();
    }
}

void SurveyConsole::RecordHole(float i, float j, float depthM)
{
    knowledge.Add(i, j, depthM);
}

int SurveyConsole::AddScour(float i, float j)
{
    scours.push_back({i, j, 0.0f});
    scourRevision++;
    return static_cast<int>(scours.size()) - 1;
}

void SurveyConsole::SetScour(int index, float i, float j, float progress)
{
    if (index < 0 || index >= static_cast<int>(scours.size())) return;
    // Quantised: the scour deepens continuously but the ground is far too
    // expensive to rebuild at 60 Hz, so it steps in about ten stages across
    // a cut while the drill and its spatter run smooth on top.
    const float stepped = std::round(std::min(std::max(progress, 0.0f), 1.0f) * 10.0f) / 10.0f;
    SurveyScour& s = scours[index];
    if (std::fabs(s.progress - stepped) < 1e-4f && std::fabs(s.i - i) < 1e-4f
        && std::fabs(s.j - j) < 1e-4f) return;
    s.i = i; s.j = j; s.progress = stepped;
    scourRevision++;
}

void SurveyConsole::Clear()
{
    knowledge.Clear();
    scours.clear();
    scourRevision++;
    block.selected = -1;
    block.explode = block.explodeTarget = 0.0f;
}
