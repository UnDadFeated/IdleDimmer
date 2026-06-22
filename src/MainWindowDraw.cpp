#include "MainWindow.h"
#include "DimmerManager.h"
#include <format>
#include <algorithm>

#ifndef D2DERR_RECREATED
#define D2DERR_RECREATED ((HRESULT)0x88980001L)
#endif

void MainWindow::OnPaint() {
    if (FAILED(CreateGraphicsResources())) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->SetTransform(D2D1::IdentityMatrix());
    
    // Dynamic Premium Slate / Grey monochrome Theme Tokens
    if (m_config.lightMode) {
        m_pBrushBg->SetColor(D2D1::ColorF(0xF0F0F2));
        m_pBrushCard->SetColor(D2D1::ColorF(0xFFFFFF));
        m_pBrushCardBorder->SetColor(D2D1::ColorF(0xE0E0E0));
        m_pBrushText->SetColor(D2D1::ColorF(0x1F1F1F));
        m_pBrushTextMuted->SetColor(D2D1::ColorF(0x757575));
        m_pBrushTrack->SetColor(D2D1::ColorF(0xE0E0E2));
        m_pBrushAccent->SetColor(D2D1::ColorF(0x7A7A7E));
        m_pBrushAccentHover->SetColor(D2D1::ColorF(0x555558));
    } else {
        // Sleek Industrial Dark Slate Theme
        m_pBrushBg->SetColor(D2D1::ColorF(0x121212));
        m_pBrushCard->SetColor(D2D1::ColorF(0x1E1E1E));
        m_pBrushCardBorder->SetColor(D2D1::ColorF(0x2D2D2D));
        m_pBrushText->SetColor(D2D1::ColorF(0xE1E1E1));
        m_pBrushTextMuted->SetColor(D2D1::ColorF(0x808080));
        m_pBrushTrack->SetColor(D2D1::ColorF(0x2D2D2D));
        m_pBrushAccent->SetColor(D2D1::ColorF(0x8E8E93));
        m_pBrushAccentHover->SetColor(D2D1::ColorF(0xC7C7CC));
    }

    // Clear screen
    m_pRenderTarget->Clear(m_pBrushBg->GetColor());

    // Render interactive cards & sliders
    for (const auto& slider : m_sliders) {
        // Draw round cornered card
        D2D1_ROUNDED_RECT cardRect = D2D1::RoundedRect(
            D2D1::RectF(slider.rect.left, slider.rect.top, slider.rect.right, slider.rect.bottom),
            8.0f, 8.0f
        );
        m_pRenderTarget->FillRoundedRectangle(cardRect, m_pBrushCard);
        
        // Active border glow! Glow with accent color if active, otherwise standard card border
        m_pRenderTarget->DrawRoundedRectangle(
            cardRect, 
            slider.active ? m_pBrushAccent : m_pBrushCardBorder, 
            slider.active ? 1.6f : 1.2f
        );

        // Find associated name and value string
        std::wstring displayName;
        wchar_t pctStr[32] = { 0 };
        if (slider.isMaster) {
            displayName = L"Master Controller (All Monitors)";
            int pct = static_cast<int>(slider.value * 90.0f);
            wcscpy_s(pctStr, ARRAYSIZE(pctStr), std::format(L"{}%", pct).c_str());
        } else if (slider.isIdleMinutes) {
            displayName = L"Inactivity Timeout";
            int mins = static_cast<int>(slider.value * 59.0f) + 1;
            wcscpy_s(pctStr, ARRAYSIZE(pctStr), std::format(L"{} min", mins).c_str());
        } else if (slider.isIdleDimLevel) {
            displayName = L"Inactivity Dim Level";
            int lvl = static_cast<int>(slider.value * 100.0f);
            wcscpy_s(pctStr, ARRAYSIZE(pctStr), std::format(L"{}%", lvl).c_str());
        } else if (slider.isScheduleStart || slider.isScheduleEnd) {
            // v1.6.5 (Todo 8): Time-of-day slider. Round to nearest 15 minutes
            // for display, mirroring the snapping done on commit.
            int totalMins = static_cast<int>(slider.value * 1439.0f);
            int snapped = ((totalMins + 7) / 15) * 15;
            if (snapped >= 1440) snapped = 1439;
            int hh = snapped / 60;
            int mm = snapped % 60;
            wcscpy_s(pctStr, ARRAYSIZE(pctStr), std::format(L"{:02d}:{:02d}", hh, mm).c_str());
            displayName = slider.isScheduleStart ? L"Schedule Start" : L"Schedule End";
        } else {
            const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
            for (const auto& mon : activeMons) {
                if (mon.id == slider.monitorId) {
                    displayName = mon.friendlyName;
                    break;
                }
            }
            int pct = static_cast<int>(slider.value * 90.0f);
            wcscpy_s(pctStr, ARRAYSIZE(pctStr), std::format(L"{}%", pct).c_str());
        }

        // Draw monitor label (shift text left if checkbox is inside the card)
        float textLeft = (slider.isIdleMinutes || slider.isIdleDimLevel ||
                          slider.isScheduleStart || slider.isScheduleEnd)
                         ? (slider.rect.left + 20.0f) : (slider.rect.left + 60.0f);
        m_pRenderTarget->DrawText(
            displayName.c_str(), static_cast<UINT32>(displayName.length()),
            m_pTextFormatBody,
            D2D1::RectF(textLeft, slider.rect.top + 14.0f, slider.rect.right - 90.0f, slider.rect.top + 34.0f),
            slider.active ? m_pBrushText : m_pBrushTextMuted
        );

        // Draw current dim % or minutes value
        m_pRenderTarget->DrawText(
            pctStr, static_cast<UINT32>(wcslen(pctStr)),
            m_pTextFormatBody,
            D2D1::RectF(slider.rect.right - 85.0f, slider.rect.top + 14.0f, slider.rect.right - 15.0f, slider.rect.top + 34.0f),
            slider.active ? m_pBrushText : m_pBrushTextMuted
        );

        // Draw track slider bar (thicker rounded techie progress bar)
        float trackY = slider.rect.bottom - 22.0f;
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;

        m_pRenderTarget->DrawLine(
            D2D1::Point2F(trackLeft, trackY),
            D2D1::Point2F(trackRight, trackY),
            m_pBrushTrack,
            6.0f
        );

        // Active progress track segment
        float thumbX = trackLeft + (slider.value * trackWidth);
        if (slider.active) {
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(trackLeft, trackY),
                D2D1::Point2F(thumbX, trackY),
                m_pBrushAccent,
                6.0f
            );
        }

        // Technical Dual-Ring Slider Knob (mixing console style)
        if (slider.active) {
            m_pRenderTarget->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 8.0f, 8.0f),
                slider.isHovered ? m_pBrushAccentHover : m_pBrushAccent
            );
            m_pRenderTarget->DrawEllipse(
                D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 10.0f, 10.0f),
                m_pBrushText,
                1.5f
            );
        } else {
            m_pRenderTarget->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(thumbX, trackY), 7.0f, 7.0f),
                m_pBrushTrack
            );
        }
    }

    // ── v1.6.5 (Todo 5): Preset Buttons Row ──
    // First draw the section header label above the first preset button.
    if (!m_presets.empty()) {
        m_pRenderTarget->DrawText(
            L"PRESETS", 7, m_pTextFormatDetail,
            D2D1::RectF(25.0f, (float)m_presets[0].rect.top - 18.0f,
                        200.0f, (float)m_presets[0].rect.top - 2.0f),
            m_pBrushTextMuted
        );

        for (const auto& btn : m_presets) {
            D2D1_ROUNDED_RECT r = D2D1::RoundedRect(
                D2D1::RectF((float)btn.rect.left, (float)btn.rect.top,
                            (float)btn.rect.right, (float)btn.rect.bottom),
                6.0f, 6.0f);
            // Hovered = accent fill (light), normal = card fill (dark).
            m_pRenderTarget->FillRoundedRectangle(r, btn.hovered ? m_pBrushAccent : m_pBrushCard);
            m_pRenderTarget->DrawRoundedRectangle(
                r, btn.hovered ? m_pBrushAccentHover : m_pBrushCardBorder, 1.2f);

            // Label color flips with hover for clear feedback.
            m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_pRenderTarget->DrawText(
                btn.label.c_str(), (UINT32)btn.label.length(),
                m_pTextFormatDetail,
                D2D1::RectF((float)btn.rect.left, (float)btn.rect.top + 6.0f,
                            (float)btn.rect.right, (float)btn.rect.bottom),
                btn.hovered ? m_pBrushText : m_pBrushTextMuted
            );
            m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    }

    // Draw grouped section header labels above their first toggle
    for (const auto& cb : m_checkboxes) {
        if (cb.settingName == L"DimmingEnabled" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"SCREEN DIMMING", 14, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        } else if (cb.settingName == L"WarmTint" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"SCREEN DISPLAY", 14, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        } else if (cb.settingName == L"CloseToTray" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"APPLICATION", 11, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        } else if (cb.settingName == L"ScheduleEnabled" && !cb.label.empty()) {
            // v1.6.5 (Todo 8): schedule section header
            m_pRenderTarget->DrawText(
                L"SCHEDULE", 8, m_pTextFormatDetail,
                D2D1::RectF(25.0f, cb.rect.top - 18.0f, 200.0f, cb.rect.top - 2.0f),
                m_pBrushTextMuted
            );
        }
    }

    // Render high-tech sliding toggle switches
    for (const auto& cb : m_checkboxes) {
        D2D1_ROUNDED_RECT switchTrack = D2D1::RoundedRect(
            D2D1::RectF(cb.rect.left, cb.rect.top, cb.rect.right, cb.rect.bottom),
            9.0f, 9.0f
        );

        if (cb.checked) {
            m_pRenderTarget->FillRoundedRectangle(switchTrack, m_pBrushAccent);
            m_pRenderTarget->DrawRoundedRectangle(switchTrack, cb.isHovered ? m_pBrushAccentHover : m_pBrushAccent, 1.2f);
        } else {
            m_pRenderTarget->FillRoundedRectangle(switchTrack, m_pBrushTrack);
            m_pRenderTarget->DrawRoundedRectangle(switchTrack, cb.isHovered ? m_pBrushAccentHover : m_pBrushCardBorder, 1.2f);
        }

        // Draw circular sliding toggle knob
        float knobX = cb.checked ? (cb.rect.left + 25.0f) : (cb.rect.left + 9.0f);
        float knobY = cb.rect.top + 9.0f;
        m_pRenderTarget->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(knobX, knobY), 6.0f, 6.0f),
            cb.checked ? m_pBrushText : m_pBrushTextMuted
        );

        if (!cb.label.empty()) {
            float labelRight = (cb.rect.left < CONTENT_WIDTH / 2) ? (CONTENT_WIDTH / 2.0f - 10.0f) : (CONTENT_WIDTH - 20.0f);
            m_pRenderTarget->DrawText(
                cb.label.c_str(), static_cast<UINT32>(cb.label.length()),
                m_pTextFormatDetail,
                D2D1::RectF(cb.rect.right + 10.0f, cb.rect.top + 1.0f, labelRight, cb.rect.bottom + 15.0f),
                m_pBrushText
            );
        }
    }

    // ── RIGHT-SIDE PANEL ──
    // Draw expansion arrow at top-right of content area
    float ax = (float)m_blockedArrowRect.left;
    float ay = (float)m_blockedArrowRect.top;
    m_pRenderTarget->DrawText(
        m_blockedExpanded ? L"\u25BC" : L"\u25B6", 1, m_pTextFormatDetail,
        D2D1::RectF(ax, ay, ax + 16, ay + 16),
        m_blockedArrowHovered ? m_pBrushAccent : m_pBrushTextMuted
    );
    m_pRenderTarget->DrawText(
        L"Bypass Apps", 11, m_pTextFormatDetail,
        D2D1::RectF(ax + 17, ay, CONTENT_WIDTH - 10, ay + 16),
        m_blockedArrowHovered ? m_pBrushAccent : m_pBrushTextMuted
    );

    if (m_blockedExpanded) {
        D2D1_ROUNDED_RECT panelRect = D2D1::RoundedRect(
            D2D1::RectF(m_blockedPanelRect.left, m_blockedPanelRect.top,
                        m_blockedPanelRect.right, m_blockedPanelRect.bottom),
            8.0f, 8.0f
        );
        m_pRenderTarget->FillRoundedRectangle(panelRect, m_pBrushCard);
        m_pRenderTarget->DrawRoundedRectangle(panelRect, m_pBrushCardBorder, 1.2f);

        float headerY = (float)m_blockedPanelRect.top + 12.0f;
        m_pRenderTarget->DrawText(
            L"Bypass Apps", 11, m_pTextFormatDetail,
            D2D1::RectF(m_blockedPanelRect.left + 12, headerY,
                        m_blockedAddRect.right - 56, headerY + 18),
            m_pBrushTextMuted
        );
        m_pRenderTarget->DrawText(
            L"[+ Add]", 6, m_pTextFormatDetail,
            D2D1::RectF(m_blockedAddRect.right - 52, headerY - 2,
                        m_blockedAddRect.right, headerY + 18),
            m_blockedAddHovered ? m_pBrushAccent : m_pBrushText
        );

        // ── v1.6.5 (Todo 6): Import / Export Profile buttons ──
        // Draw two small compact buttons between the panel header and the
        // separator line. The right panel's content y-offset was already
        // shifted down by 26px in UpdateLayout to make room for these.
        auto DrawProfileBtn = [&](const RECT& r, bool hovered, const wchar_t* label, int labelLen) {
            D2D1_ROUNDED_RECT pr = D2D1::RoundedRect(
                D2D1::RectF((float)r.left, (float)r.top,
                            (float)r.right, (float)r.bottom),
                4.0f, 4.0f);
            m_pRenderTarget->FillRoundedRectangle(pr, hovered ? m_pBrushAccent : m_pBrushTrack);
            m_pRenderTarget->DrawRoundedRectangle(
                pr, hovered ? m_pBrushAccentHover : m_pBrushCardBorder, 1.0f);
            m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_pRenderTarget->DrawText(
                label, labelLen, m_pTextFormatDetail,
                D2D1::RectF((float)r.left, (float)r.top + 4.0f,
                            (float)r.right, (float)r.bottom),
                hovered ? m_pBrushText : m_pBrushTextMuted
            );
            m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        };
        DrawProfileBtn(m_importProfileRect, m_importProfileHovered, L"Import",  6);
        DrawProfileBtn(m_exportProfileRect, m_exportProfileHovered, L"Export",  6);

        float sepY = headerY + 26 + 32; // shift separator down to clear profile row
        m_pRenderTarget->DrawLine(
            D2D1::Point2F(m_blockedPanelRect.left + 12, sepY),
            D2D1::Point2F(m_blockedPanelRect.right - 12, sepY),
            m_pBrushCardBorder, 1.0f
        );

        float clipTop = sepY + 2;
        float clipBottom = m_blockedPanelRect.bottom - 4;
        m_pRenderTarget->PushAxisAlignedClip(
            D2D1::RectF(m_blockedPanelRect.left, clipTop,
                        m_blockedPanelRect.right, clipBottom),
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
        );
        m_pRenderTarget->SetTransform(
            D2D1::Matrix3x2F::Translation(0.0f, -(float)m_blockedScrollOffset)
        );

        for (const auto& item : m_blockedItems) {
            m_pRenderTarget->DrawText(
                item.name.c_str(), (UINT32)item.name.length(), m_pTextFormatBody,
                D2D1::RectF(item.textRect.left, item.textRect.top,
                           item.textRect.right, item.textRect.bottom),
                m_pBrushText
            );
            m_pRenderTarget->DrawText(
                L"\u2715", 1, m_pTextFormatDetail,
                D2D1::RectF(item.removeRect.left + 3, item.removeRect.top + 1,
                           item.removeRect.left + 19, item.removeRect.top + 17),
                item.hoveredRemove ? m_pBrushAccent : m_pBrushTextMuted
            );
        }

        m_pRenderTarget->SetTransform(D2D1::IdentityMatrix());
        m_pRenderTarget->PopAxisAlignedClip();
    }

    // Technical Separator Line before Footer Metadata
    float footerY = static_cast<float>(m_windowHeight - 35);
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(20.0f, footerY),
        D2D1::Point2F(CONTENT_WIDTH - 35.0f, footerY),
        m_pBrushCardBorder,
        1.0f
    );

    // Dynamic Dimmer Status Indicator
    bool anyDimmerActive = false;
    if (m_config.dimmingEnabled) {
        const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
        for (const auto& mon : activeMons) {
            if (mon.enabled && mon.dimValue > 0) {
                anyDimmerActive = true;
                break;
            }
        }
    }

    wchar_t statusStr[64] = { 0 };
    if (anyDimmerActive) {
        wcscpy_s(statusStr, ARRAYSIZE(statusStr), L"SYSTEM: ACTIVE");
    } else {
        wcscpy_s(statusStr, ARRAYSIZE(statusStr), L"SYSTEM: STANDBY");
    }

    m_pRenderTarget->DrawText(
        statusStr, static_cast<UINT32>(wcslen(statusStr)),
        m_pTextFormatDetail,
        D2D1::RectF(25.0f, footerY + 10.0f, 250.0f, footerY + 28.0f),
        anyDimmerActive ? m_pBrushAccent : m_pBrushTextMuted
    );

    // Undo Changes centered in footer
    wchar_t undoLabel[32] = { 0 };
    if (m_changeCount > 0) {
        wcscpy_s(undoLabel, ARRAYSIZE(undoLabel), std::format(L"Undo ({})", m_changeCount).c_str());
    } else {
        wcscpy_s(undoLabel, ARRAYSIZE(undoLabel), L"Undo Changes");
    }
    m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    m_pRenderTarget->DrawText(
        undoLabel, static_cast<UINT32>(wcslen(undoLabel)),
        m_pTextFormatDetail,
        D2D1::RectF(m_undoRect.left, footerY + 10.0f, m_undoRect.right, footerY + 28.0f),
        m_canUndo ? m_pBrushAccent : m_pBrushTextMuted
    );
    m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    // Version Number in footer right
    wchar_t versionFull[64] = { 0 };
    if (m_updateChecked) {
        if (m_updateAvailable) {
            wcscpy_s(versionFull, ARRAYSIZE(versionFull), std::format(L"Update Available | {}", m_latestVersion).c_str());
        } else {
            wcscpy_s(versionFull, ARRAYSIZE(versionFull), std::format(L"Up to Date | {}", m_appVersion).c_str());
        }
    } else {
        wcscpy_s(versionFull, ARRAYSIZE(versionFull), m_appVersion.c_str());
    }
    m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    m_pRenderTarget->DrawText(
        versionFull, static_cast<UINT32>(wcslen(versionFull)),
        m_pTextFormatDetail,
        D2D1::RectF(CONTENT_WIDTH - 170.0f, footerY + 10.0f, CONTENT_WIDTH - 25.0f, footerY + 28.0f),
        m_updateAvailable ? m_pBrushAccent : m_pBrushTextMuted
    );
    m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATED) {
        DiscardGraphicsResources();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}
