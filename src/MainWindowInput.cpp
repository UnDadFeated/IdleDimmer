#define NOMINMAX
#include "MainWindow.h"
#include "DimmerManager.h"
#include <dwmapi.h>
#include <algorithm>
#include <commdlg.h>
#include <format>

void MainWindow::HandleMouseMove(int x, int y) {
    bool needsRepaint = false;

    // Sliders check
    for (auto& slider : m_sliders) {
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;
        float trackY = slider.rect.bottom - 22.0f;

        if (slider.isDragging && slider.active) {
            float relX = static_cast<float>(x) - trackLeft;
            slider.value = relX / trackWidth;
            if (slider.value < 0.0f) slider.value = 0.0f;
            if (slider.value > 1.0f) slider.value = 1.0f;

            // Auto-enable dimming when user actively drags a monitor slider
            if (!slider.isIdleMinutes && !slider.isIdleDimLevel &&
                !slider.isScheduleStart && !slider.isScheduleEnd &&
                !m_config.dimmingEnabled) {
                m_config.dimmingEnabled = true;
                DimmerManager::Instance().SetDimmingEnabled(true);
                for (auto& cb : m_checkboxes) {
                    if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                }
            }

            if (slider.isMaster) {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                m_config.masterValue = actualDim;
                const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                for (const auto& mon : activeMons) {
                    DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.value = actualDim;
                }
                for (auto& other : m_sliders) {
                    if (!other.isMaster && !other.isIdleMinutes && !other.isIdleDimLevel &&
                        !other.isScheduleStart && !other.isScheduleEnd) {
                        other.value = slider.value;
                    }
                }
                // v1.6.5 (Todo 4): trigger OSD on master drag.
                DimmerManager::Instance().ShowOSD(std::format(L"Brightness: {}%", 100 - actualDim));
            } else if (slider.isIdleMinutes) {
                m_config.idleMinutes = static_cast<int>(slider.value * 59.0f) + 1;
            } else if (slider.isIdleDimLevel) {
                m_config.idleDimLevel = static_cast<int>(slider.value * 100.0f);
                if (DimmerManager::Instance().IsIdleState()) {
                    DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                }
            } else if (slider.isScheduleStart) {
                // v1.6.5 (Todo 8): snap to nearest 15 minutes.
                int totalMins = static_cast<int>(slider.value * 1439.0f);
                int snapped = ((totalMins + 7) / 15) * 15;
                if (snapped >= 1440) snapped = 1439;
                m_config.scheduleStartMins = snapped;
                DimmerManager::Instance().SetScheduleRange(
                    m_config.scheduleStartMins,
                    m_config.scheduleEndMins,
                    m_config.scheduleDimLevel);
                DimmerManager::Instance().CheckSchedule();
            } else if (slider.isScheduleEnd) {
                int totalMins = static_cast<int>(slider.value * 1439.0f);
                int snapped = ((totalMins + 7) / 15) * 15;
                if (snapped >= 1440) snapped = 1439;
                m_config.scheduleEndMins = snapped;
                DimmerManager::Instance().SetScheduleRange(
                    m_config.scheduleStartMins,
                    m_config.scheduleEndMins,
                    m_config.scheduleDimLevel);
                DimmerManager::Instance().CheckSchedule();
            } else {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                for (auto& monConf : m_config.monitors) {
                    if (monConf.id == slider.monitorId) {
                        monConf.value = actualDim;
                        break;
                    }
                }
                // v1.6.5 (Todo 4): OSD on per-monitor drag too.
                DimmerManager::Instance().ShowOSD(std::format(L"Monitor: {}%", 100 - actualDim));
            }
            needsRepaint = true;
        }

        // Hover checking
        float thumbX = trackLeft + (slider.value * trackWidth);
        bool wasHovered = slider.isHovered;
        slider.isHovered = (abs(x - thumbX) < 12 && abs(y - trackY) < 12);
        if (slider.isHovered != wasHovered) {
            needsRepaint = true;
        }
    }

    // Checkboxes check
    for (auto& cb : m_checkboxes) {
        bool wasHovered = cb.isHovered;
        cb.isHovered = (x >= cb.rect.left && x <= cb.rect.right && y >= cb.rect.top && y <= cb.rect.bottom);
        if (cb.isHovered != wasHovered) {
            needsRepaint = true;
        }
    }

    // v1.6.5 (Todo 5): preset button hover detection
    for (auto& btn : m_presets) {
        bool was = btn.hovered;
        btn.hovered = (x >= btn.rect.left && x <= btn.rect.right &&
                       y >= btn.rect.top && y <= btn.rect.bottom);
        if (btn.hovered != was) needsRepaint = true;
    }

    // v1.6.5 (Todo 6): Import/Export profile button hover
    bool wasImport = m_importProfileHovered;
    m_importProfileHovered = m_blockedExpanded &&
        (x >= m_importProfileRect.left && x <= m_importProfileRect.right &&
         y >= m_importProfileRect.top && y <= m_importProfileRect.bottom);
    if (m_importProfileHovered != wasImport) needsRepaint = true;
    bool wasExport = m_exportProfileHovered;
    m_exportProfileHovered = m_blockedExpanded &&
        (x >= m_exportProfileRect.left && x <= m_exportProfileRect.right &&
         y >= m_exportProfileRect.top && y <= m_exportProfileRect.bottom);
    if (m_exportProfileHovered != wasExport) needsRepaint = true;

    // Blocked apps hover
    bool wasArrowHover = m_blockedArrowHovered;
    m_blockedArrowHovered = (x >= m_blockedArrowRect.left && x <= m_blockedArrowRect.right && y >= m_blockedArrowRect.top && y <= m_blockedArrowRect.bottom);
    if (m_blockedArrowHovered != wasArrowHover) needsRepaint = true;

    bool wasAddHover = m_blockedAddHovered;
    m_blockedAddHovered = m_blockedExpanded && (x >= m_blockedAddRect.left && x <= m_blockedAddRect.right && y >= m_blockedAddRect.top && y <= m_blockedAddRect.bottom);
    if (m_blockedAddHovered != wasAddHover) needsRepaint = true;

    // Panel items hover (adjusted for scroll offset)
    int adjustedY = y + m_blockedScrollOffset;
    for (auto& item : m_blockedItems) {
        bool wasHover = item.hoveredRemove;
        item.hoveredRemove = (x >= item.removeRect.left && x <= item.removeRect.right &&
                             adjustedY >= item.removeRect.top && adjustedY <= item.removeRect.bottom);
        if (item.hoveredRemove != wasHover) needsRepaint = true;
    }

    if (needsRepaint) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::HandleLButtonDown(int x, int y) {
    // Intercept Undo click in footer
    if (m_canUndo && x >= m_undoRect.left && x <= m_undoRect.right && y >= m_undoRect.top && y <= m_undoRect.bottom) {
        m_config = m_undoStack.back();
        m_undoStack.pop_back();
        m_canUndo = !m_undoStack.empty();
        m_changeCount = static_cast<int>(m_undoStack.size());

        SyncMonitorsWithConfig();

        const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
        for (const auto& mon : activeMons) {
            for (const auto& savedMon : m_config.monitors) {
                if (savedMon.id == mon.id) {
                    DimmerManager::Instance().SetMonitorDim(mon.id, savedMon.value);
                    DimmerManager::Instance().SetMonitorEnabled(mon.id, savedMon.enabled);
                    break;
                }
            }
        }
        DimmerManager::Instance().SetWarmTint(m_config.warmTint);
        DimmerManager::Instance().SetBlockedApps(m_config.blockedApps);
        if (!m_config.idleDimEnabled) {
            DimmerManager::Instance().SetIdleState(false);
        }
        DimmerManager::Instance().SetDimmingEnabled(m_config.dimmingEnabled);

        BOOL useDark = !m_config.lightMode;
        DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        UpdateLayout();
        SaveSettings();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    // Blocked apps interactions
    if (x >= m_blockedArrowRect.left && x <= m_blockedArrowRect.right && y >= m_blockedArrowRect.top && y <= m_blockedArrowRect.bottom) {
        m_blockedExpanded = !m_blockedExpanded;
        if (!m_blockedExpanded) m_blockedScrollOffset = 0;
        UpdateLayout();
        Repaint();
        return;
    }
    if (m_blockedExpanded && x >= m_blockedAddRect.left && x <= m_blockedAddRect.right && y >= m_blockedAddRect.top && y <= m_blockedAddRect.bottom) {
        ShowAddAppDialog();
        return;
    }
    // v1.6.5 (Todo 6): Import / Export profile buttons
    if (m_blockedExpanded &&
        x >= m_importProfileRect.left && x <= m_importProfileRect.right &&
        y >= m_importProfileRect.top && y <= m_importProfileRect.bottom) {
        ShowImportProfileDialog();
        return;
    }
    if (m_blockedExpanded &&
        x >= m_exportProfileRect.left && x <= m_exportProfileRect.right &&
        y >= m_exportProfileRect.top && y <= m_exportProfileRect.bottom) {
        ShowExportProfileDialog();
        return;
    }
    if (m_blockedExpanded) {
        int adjustedY = y + m_blockedScrollOffset;
        for (size_t i = 0; i < m_blockedItems.size(); ++i) {
            if (x >= m_blockedItems[i].removeRect.left && x <= m_blockedItems[i].removeRect.right &&
                adjustedY >= m_blockedItems[i].removeRect.top && adjustedY <= m_blockedItems[i].removeRect.bottom) {
                m_config.blockedApps.erase(m_config.blockedApps.begin() + i);
                DimmerManager::Instance().SetBlockedApps(m_config.blockedApps);
                UpdateLayout();
                SaveSettings();
                Repaint();
                return;
            }
        }
    }

    // v1.6.5 (Todo 5): preset button clicks. Must be checked BEFORE the
    // slider-drag block — preset buttons sit in the same y-band as the
    // slider track hot zone, and we don't want a preset click to start a drag.
    for (auto& btn : m_presets) {
        if (x >= btn.rect.left && x <= btn.rect.right &&
            y >= btn.rect.top  && y <= btn.rect.bottom) {
            PushUndoState();
            ApplyPreset(btn.id);
            SaveSettings();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
        }
    }

    for (auto& slider : m_sliders) {
        float trackLeft = slider.rect.left + 20.0f;
        float trackRight = slider.rect.right - 20.0f;
        float trackWidth = trackRight - trackLeft;
        float trackY = slider.rect.bottom - 22.0f;

        float thumbX = trackLeft + (slider.value * trackWidth);
        // If clicked anywhere near the track or the thumb
        if (slider.active && x >= trackLeft - 5 && x <= trackRight + 5 && abs(y - trackY) < 10) {
            PushUndoState();
            slider.isDragging = true;
            m_isDraggingAny = true;
            SetCapture(m_hwnd);
            HandleMouseMove(x, y); // Immediately position thumb to mouse
            break;
        }
    }

    // Checkboxes check
    for (auto& cb : m_checkboxes) {
        if (x >= cb.rect.left && x <= cb.rect.right && y >= cb.rect.top && y <= cb.rect.bottom) {
            PushUndoState();
            cb.checked = !cb.checked;
            
            // Dynamic configuration binding assignment
            if (cb.pValue) {
                *cb.pValue = cb.checked;
            }

            // Apply setting immediately
            if (!cb.monitorId.empty()) {
                DimmerManager::Instance().SetMonitorEnabled(cb.monitorId, cb.checked);
                for (auto& monConf : m_config.monitors) {
                    if (monConf.id == cb.monitorId) {
                        monConf.enabled = cb.checked;
                        break;
                    }
                }
                for (auto& sl : m_sliders) {
                    if (sl.monitorId == cb.monitorId) {
                        sl.active = cb.checked;
                        break;
                    }
                }
            } else if (cb.settingName == L"MasterEnabled") {
                // Enable/disable all individual monitor sliders
                const auto& activeMons = DimmerManager::Instance().GetActiveMonitors();
                for (const auto& mon : activeMons) {
                    DimmerManager::Instance().SetMonitorEnabled(mon.id, cb.checked);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.enabled = cb.checked;
                }
                for (auto& sl : m_sliders) {
                    if (!sl.isIdleMinutes && !sl.isIdleDimLevel) {
                        sl.active = cb.checked;
                    }
                }
                for (auto& otherCb : m_checkboxes) {
                    if (!otherCb.monitorId.empty()) {
                        otherCb.checked = cb.checked;
                    }
                }
            } else if (cb.settingName == L"CloseToTray") {
                // Handled dynamically
            } else if (cb.settingName == L"StartWithWindows") {
                ToggleStartWithWindows(cb.checked);
            } else if (cb.settingName == L"WarmTint") {
                DimmerManager::Instance().SetWarmTint(cb.checked);
            } else if (cb.settingName == L"IdleDimEnabled") {
                if (!cb.checked) {
                    DimmerManager::Instance().SetIdleState(false);
                }
                UpdateLayout();
            } else if (cb.settingName == L"IdleTurnOff") {
                // Handled dynamically
            } else if (cb.settingName == L"DimmingEnabled") {
                DimmerManager::Instance().SetDimmingEnabled(cb.checked);
            } else if (cb.settingName == L"ScheduleEnabled") {
                // v1.6.5 (Todo 8): push to DimmerManager and re-layout so
                // the start/end sliders appear or disappear.
                DimmerManager::Instance().SetScheduleEnabled(cb.checked);
                DimmerManager::Instance().CheckSchedule();
                UpdateLayout();
            } else if (cb.settingName == L"LightMode") {
                BOOL useDark = !cb.checked;
                DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
                SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }

            SaveSettings();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            break;
        }
    }
}

void MainWindow::HandleLButtonUp(int x, int y) {
    if (m_isDraggingAny) {
        for (auto& slider : m_sliders) {
            if (slider.isDragging) {
                slider.isDragging = false;
                break;
            }
        }
        m_isDraggingAny = false;
        ReleaseCapture();
        SaveSettings();
    }
}

void MainWindow::HandleMouseWheel(short delta, int x, int y) {
    POINT pt = { x, y };
    ScreenToClient(m_hwnd, &pt);

    // Panel scroll: if mouse is over the right-side panel
    if (m_blockedExpanded) {
        if (pt.x >= m_blockedPanelRect.left && pt.x <= m_blockedPanelRect.right &&
            pt.y >= m_blockedPanelRect.top && pt.y <= m_blockedPanelRect.bottom) {
            int visibleHeight = m_blockedPanelRect.bottom - m_blockedPanelRect.top - 60;
            int maxOffset = (std::max)(0, m_blockedContentHeight - visibleHeight);
            int step = (delta > 0) ? -30 : 30;
            m_blockedScrollOffset += step;
            if (m_blockedScrollOffset < 0) m_blockedScrollOffset = 0;
            if (m_blockedScrollOffset > maxOffset) m_blockedScrollOffset = maxOffset;
            Repaint();
            return;
        }
    }

    // Zoom/increment the slider currently hovered
    bool changed = false;
    for (auto& slider : m_sliders) {
        if (slider.active && pt.x >= slider.rect.left && pt.x <= slider.rect.right && pt.y >= slider.rect.top && pt.y <= slider.rect.bottom) {
            PushUndoState();
            float step = (delta > 0) ? 0.02f : -0.02f;
            slider.value += step;
            if (slider.value < 0.0f) slider.value = 0.0f;
            if (slider.value > 1.0f) slider.value = 1.0f;

            // Auto-enable dimming when user scrolls a monitor slider
            if (!slider.isIdleMinutes && !slider.isIdleDimLevel &&
                !slider.isScheduleStart && !slider.isScheduleEnd &&
                !m_config.dimmingEnabled) {
                m_config.dimmingEnabled = true;
                DimmerManager::Instance().SetDimmingEnabled(true);
                for (auto& cb : m_checkboxes) {
                    if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                }
            }

            if (slider.isMaster) {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                m_config.masterValue = actualDim;
                for (auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                    DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                }
                for (auto& monConf : m_config.monitors) {
                    monConf.value = actualDim;
                }
                for (auto& other : m_sliders) {
                    if (!other.isMaster && !other.isIdleMinutes && !other.isIdleDimLevel &&
                        !other.isScheduleStart && !other.isScheduleEnd) other.value = slider.value;
                }
            } else if (slider.isIdleMinutes) {
                m_config.idleMinutes = static_cast<int>(slider.value * 59.0f) + 1;
            } else if (slider.isIdleDimLevel) {
                m_config.idleDimLevel = static_cast<int>(slider.value * 100.0f);
                if (DimmerManager::Instance().IsIdleState()) {
                    DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                }
            } else if (slider.isScheduleStart) {
                int totalMins = static_cast<int>(slider.value * 1439.0f);
                int snapped = ((totalMins + 7) / 15) * 15;
                if (snapped >= 1440) snapped = 1439;
                m_config.scheduleStartMins = snapped;
                DimmerManager::Instance().SetScheduleRange(
                    m_config.scheduleStartMins, m_config.scheduleEndMins, m_config.scheduleDimLevel);
                DimmerManager::Instance().CheckSchedule();
            } else if (slider.isScheduleEnd) {
                int totalMins = static_cast<int>(slider.value * 1439.0f);
                int snapped = ((totalMins + 7) / 15) * 15;
                if (snapped >= 1440) snapped = 1439;
                m_config.scheduleEndMins = snapped;
                DimmerManager::Instance().SetScheduleRange(
                    m_config.scheduleStartMins, m_config.scheduleEndMins, m_config.scheduleDimLevel);
                DimmerManager::Instance().CheckSchedule();
            } else {
                int actualDim = static_cast<int>(slider.value * 90.0f);
                DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                for (auto& monConf : m_config.monitors) {
                    if (monConf.id == slider.monitorId) {
                        monConf.value = actualDim;
                        break;
                    }
                }
            }
            changed = true;
            break;
        }
    }

    if (changed) {
        SaveSettings();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void MainWindow::HandleKeyDown(WPARAM key) {
    // Keyboard accessibility: Left/Right arrow keys adjust active slider
    bool changed = false;
    for (auto& slider : m_sliders) {
        // If slider is hovered (focused by mouse position) or active
        if (slider.active && slider.isHovered) {
            float step = 0.0f;
            if (key == VK_RIGHT || key == VK_UP) step = 0.02f;
            else if (key == VK_LEFT || key == VK_DOWN) step = -0.02f;

            if (step != 0.0f) {
                PushUndoState();
                slider.value += step;
                if (slider.value < 0.0f) slider.value = 0.0f;
                if (slider.value > 1.0f) slider.value = 1.0f;

                // Auto-enable dimming when user arrow-keys a monitor slider
                if (!slider.isIdleMinutes && !slider.isIdleDimLevel &&
                    !slider.isScheduleStart && !slider.isScheduleEnd &&
                    !m_config.dimmingEnabled) {
                    m_config.dimmingEnabled = true;
                    DimmerManager::Instance().SetDimmingEnabled(true);
                    for (auto& cb : m_checkboxes) {
                        if (cb.settingName == L"DimmingEnabled") { cb.checked = true; break; }
                    }
                }

                if (slider.isMaster) {
                    int actualDim = static_cast<int>(slider.value * 90.0f);
                    m_config.masterValue = actualDim;
                    for (auto& mon : DimmerManager::Instance().GetActiveMonitors()) {
                        DimmerManager::Instance().SetMonitorDim(mon.id, actualDim);
                    }
                    for (auto& monConf : m_config.monitors) {
                        monConf.value = actualDim;
                    }
                    for (auto& other : m_sliders) {
                        if (!other.isMaster && !other.isIdleMinutes && !other.isIdleDimLevel &&
                            !other.isScheduleStart && !other.isScheduleEnd) other.value = slider.value;
                    }
                } else if (slider.isIdleMinutes) {
                    m_config.idleMinutes = static_cast<int>(slider.value * 59.0f) + 1;
                } else if (slider.isIdleDimLevel) {
                    m_config.idleDimLevel = static_cast<int>(slider.value * 100.0f);
                    if (DimmerManager::Instance().IsIdleState()) {
                        DimmerManager::Instance().SetIdleState(true, m_config.idleDimLevel);
                    }
                } else if (slider.isScheduleStart) {
                    int totalMins = static_cast<int>(slider.value * 1439.0f);
                    int snapped = ((totalMins + 7) / 15) * 15;
                    if (snapped >= 1440) snapped = 1439;
                    m_config.scheduleStartMins = snapped;
                    DimmerManager::Instance().SetScheduleRange(
                        m_config.scheduleStartMins, m_config.scheduleEndMins, m_config.scheduleDimLevel);
                    DimmerManager::Instance().CheckSchedule();
                } else if (slider.isScheduleEnd) {
                    int totalMins = static_cast<int>(slider.value * 1439.0f);
                    int snapped = ((totalMins + 7) / 15) * 15;
                    if (snapped >= 1440) snapped = 1439;
                    m_config.scheduleEndMins = snapped;
                    DimmerManager::Instance().SetScheduleRange(
                        m_config.scheduleStartMins, m_config.scheduleEndMins, m_config.scheduleDimLevel);
                    DimmerManager::Instance().CheckSchedule();
                } else {
                    int actualDim = static_cast<int>(slider.value * 90.0f);
                    DimmerManager::Instance().SetMonitorDim(slider.monitorId, actualDim);
                    for (auto& monConf : m_config.monitors) {
                        if (monConf.id == slider.monitorId) {
                            monConf.value = actualDim;
                            break;
                        }
                    }
                }
                changed = true;
                break;
            }
        }
    }

    if (changed) {
        SaveSettings();
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}
