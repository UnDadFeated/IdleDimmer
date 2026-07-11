#include "MainWindow.h"
#include "DimmerManager.h"
#include <format>
#include <algorithm>

#ifndef D2DERR_RECREATED
#define D2DERR_RECREATED ((HRESULT)0x88980001L)
#endif

void MainWindow::OnPaint() {
    // Safety net: if D2D has never been successfully initialized, use GDI so the
    // window always paints something and the message pump never stalls on first
    // paint. WM_APP+4 will be posted to attempt async D2D init after this paint.
    if (!m_d2dReady) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        HBRUSH bgBrush = CreateSolidBrush(m_config.lightMode ? RGB(0xF0,0xF0,0xF2) : RGB(0x12,0x12,0x12));
        FillRect(hdc, &ps.rcPaint, bgBrush);
        DeleteObject(bgBrush);
        EndPaint(m_hwnd, &ps);
        if (!m_d2dFailed) {
            PostMessageW(m_hwnd, WM_APP + 4, 0, 0);
        }
        return;
    }

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
            int lvl = static_cast<int>(slider.value * 90.0f);
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

    // Draw grouped section header labels above their first toggle
    for (const auto& cb : m_checkboxes) {
        if (cb.settingName == L"DimmingEnabled" && !cb.label.empty()) {
            m_pRenderTarget->DrawText(
                L"SCREEN DIMMING", 14, m_pTextFormatDetail,
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

    // Technical Separator Line before Footer Metadata
    float footerY = static_cast<float>(m_windowHeight - 35);
    m_pRenderTarget->DrawLine(
        D2D1::Point2F(20.0f, footerY),
        D2D1::Point2F(CONTENT_WIDTH - 35.0f, footerY),
        m_pBrushCardBorder,
        1.0f
    );

    // Dynamic Status Indicator
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

    std::wstring statusStr = DimmerManager::Instance().GetStatusString();
    int countdown = DimmerManager::Instance().GetVideoCheckCountdown();
    std::wstring statusLabel = L"Status: " + statusStr +
        L" (" + std::to_wstring(countdown) + L"s)";

    m_pRenderTarget->DrawText(
        statusLabel.c_str(), static_cast<UINT32>(statusLabel.length()),
        m_pTextFormatDetail,
        D2D1::RectF(25.0f, footerY + 10.0f, 250.0f, footerY + 28.0f),
        anyDimmerActive || DimmerManager::Instance().IsVideoDetected() || DimmerManager::Instance().IsAudioVideoDetected() ? m_pBrushAccent : m_pBrushTextMuted
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

    // Version Number / Check for Updates in footer right
    wchar_t versionFull[64] = { 0 };
    bool isUpdateAccentColor = false;

    if (m_updateChecking) {
        wcscpy_s(versionFull, ARRAYSIZE(versionFull), L"Checking updates...");
    } else if (m_updateChecked) {
        if (m_updateAvailable) {
            wcscpy_s(versionFull, ARRAYSIZE(versionFull), std::format(L"Update Available | {}", m_latestVersion).c_str());
            isUpdateAccentColor = true;
        } else {
            wcscpy_s(versionFull, ARRAYSIZE(versionFull), std::format(L"Up to Date | {}", m_appVersion).c_str());
        }
    } else {
        wcscpy_s(versionFull, ARRAYSIZE(versionFull), std::format(L"Check for Updates ({})", m_appVersion).c_str());
        isUpdateAccentColor = true; // highlight to draw attention as a button
    }

    m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    m_pRenderTarget->DrawText(
        versionFull, static_cast<UINT32>(wcslen(versionFull)),
        m_pTextFormatDetail,
        D2D1::RectF(CONTENT_WIDTH - 220.0f, footerY + 10.0f, CONTENT_WIDTH - 25.0f, footerY + 28.0f),
        isUpdateAccentColor ? m_pBrushAccent : m_pBrushTextMuted
    );
    m_pTextFormatDetail->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATED) {
        DiscardGraphicsResources();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}
