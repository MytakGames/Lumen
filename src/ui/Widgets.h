#pragma once

#include <functional>
#include <string>

#include "imgui.h"

// Custom widgets drawn on top of Dear ImGui. Pure presentation: no business logic.
namespace rl::ui {

// --- animation -------------------------------------------------------------
void setAnimationsEnabled(bool enabled);
bool animationsEnabled();
// Exponentially approaches `target` for the value stored under `id`.
// Requests extra frames while still moving so the idle loop keeps rendering.
float animate(ImGuiID id, float target, float speed = 14.0f);
float easeOutCubic(float t);

// Frame scheduling: the app sleeps when nothing changes.
void requestRedraw();
bool consumeRedrawRequest();

// --- icons -----------------------------------------------------------------
enum class Icon { Home, Accounts, Instances, Settings, Logo };
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 center, float size, ImU32 color);

// --- building blocks -------------------------------------------------------
float dp(float v);  // scales a 96-DPI value
void setDpiScale(float scale);

bool navItem(const char* id, const char* label, Icon icon, bool selected);
void pageHeader(const char* title, const char* subtitle);
void sectionTitle(const char* text);

void beginCard(const char* id);
void endCard();

bool toggle(const char* id, bool* value);

enum class ButtonKind { Primary, Secondary, Danger };
bool button(const char* label, const char* id, ButtonKind kind = ButtonKind::Secondary, bool enabled = true);
float buttonWidth(const char* label);
// Places the next item on the same line if `nextWidth` still fits, otherwise wraps.
void flowNext(float nextWidth);

// Label + muted description on the left, `control` right-aligned in `controlWidth`.
void settingRow(const char* label, const char* description, float controlWidth, const std::function<void()>& control);

void mutedText(const char* text);
void wrappedText(const char* text, const ImVec4& color);

// --- toasts ----------------------------------------------------------------
enum class ToastKind { Info, Success, Warning, Error };
void pushToast(std::string text, ToastKind kind, float seconds = 5.0f);
void drawToasts();

}  // namespace rl::ui
