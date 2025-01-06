#include "ui.hpp"

#include <type_traits>



namespace foundation {
    const u32 boldFontIndex = 1;
    static i32 s_UIContextID = 0;
    static i32 s_Counter = 0;
    std::array<char, 16> IDBuffer;
    std::array<char, 256> buffer;
    static bool setNextItemDisabled = false;

    static constexpr f32 dataLimitMin = 0.0f;
    static constexpr f32 dataLimitMax = 0.0f;
    static constexpr f32 dataDragSpeed = 0.1f;
    static constexpr f32 step = 1.0f;
    static constexpr f32 stepFast = 10.0f;
    static std::string format = ".2";

    static bool drawnAnyProperties = false;
    static ImGuiTableFlags currentPropertyTableFlags = 0;
    namespace GUI {
        void generate_ID() {
            IDBuffer[0] = '#';
            IDBuffer[1] = '#';
            std::memset(IDBuffer.data() + 2, 0, 14);
            ++s_Counter;
            std::string str_buffer = "##" + std::to_string(s_Counter);
            std::memcpy(&IDBuffer, str_buffer.data(), 16);
        }

        void external_push_ID() {
            ++s_UIContextID;
            ImGui::PushID(s_UIContextID);
            s_Counter = 0;
        }

        void ExternalPopID() {
            ImGui::PopID();
            --s_UIContextID;
        }

        void push_deactivated_status() {
            setNextItemDisabled = true;
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        void pop_deactivated_status() {
            setNextItemDisabled = false;
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        void indent(f32 width, f32 height) {
            ImGui::Dummy(ImVec2(width, height));
        }

        void show_tooltip(const char* tooltip) {
            if (s_cast<bool>(tooltip) && ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 1.0f) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(tooltip);
                ImGui::EndTooltip();
            }
        }

        auto rounded_button(const char *label, const ImVec2 &given_size, const ImDrawFlags rounding_type, const f32 rounding, ImGuiButtonFlags flags) -> bool {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems) { return false; }

            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;
            const ImGuiID id = window->GetID(label);
            const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

            ImVec2 pos = window->DC.CursorPos;
            if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) {
                pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
            }
            ImVec2 size = ImGui::CalcItemSize(given_size, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

            const ImRect bb(pos, pos + size);
            ImGui::ItemSize(size, style.FramePadding.y);
            if (!ImGui::ItemAdd(bb, id)) { return false; }

            bool repeat = g.LastItemData.ItemFlags & ImGuiItemFlags_ButtonRepeat;
            bool hovered = false;
            bool held = false;

            if(repeat) { ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true); }
            bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);
            if(repeat) { ImGui::PopItemFlag(); }
            // Render
            const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            ImGui::RenderNavHighlight(bb, id);
            ImGui::GetWindowDrawList()->AddRectFilled(bb.Min, bb.Max, col, rounding, rounding_type);

            if (g.LogEnabled) {
                ImGui::LogSetNextTextDecoration("[", "]");
            }

            ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, nullptr, &label_size, style.ButtonTextAlign, &bb);

            return pressed;
        }

        void begin_properties(const ImGuiTableFlags tableFlags) {
            bool status_changed = false;
            if (setNextItemDisabled) {
                pop_deactivated_status();
                status_changed = true;
            }

            generate_ID();
            ImGui::Unindent(GImGui->Style.IndentSpacing * 0.5f);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { GImGui->Style.CellPadding.x, GImGui->Style.CellPadding.y / 2.0f });
            ImGui::BeginTable(IDBuffer.data(), 2, tableFlags | ImGuiTableFlags_SizingFixedFit);
            ImGui::TableSetupColumn("PropertyName");
            ImGui::TableSetupColumn("PropertyData", ImGuiTableColumnFlags_WidthStretch);
            currentPropertyTableFlags = tableFlags;

            if (status_changed) {
                push_deactivated_status();
            }
        }

        void end_properties() {
            ImGui::EndTable();
            ImGui::PopStyleVar();
            ImGui::Indent(GImGui->Style.IndentSpacing * 0.5f);
            drawnAnyProperties = false;
        }

        void begin_property(const char *label) {
            external_push_ID();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushID(label);

            bool status_changed = false;
            if (setNextItemDisabled) {
                pop_deactivated_status();
                status_changed = true;
            }

            ImGui::Text("%s", label);

            if (status_changed) {
                push_deactivated_status();
            }

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            generate_ID();

            drawnAnyProperties = true;
        }

        void end_property() {
            ImGui::PopID();
            ExternalPopID();
        }

        template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        static inline auto GetNumericImGuiDataType() -> ImGuiDataType {
            if (std::is_same<T, i32>()) { return ImGuiDataType_S32; }
            if (std::is_same<T, u32>()) { return ImGuiDataType_U32; }
            if (std::is_same<T, i64>()) { return ImGuiDataType_S64; }
            if (std::is_same<T, u64>()) { return ImGuiDataType_U64; }
            if (std::is_same<T, f32>()) { return ImGuiDataType_Float; }
            if (std::is_same<T, f64>()) { return ImGuiDataType_Double; }
            return -1;
        }

        template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        static inline auto GetSuitableFormat() -> std::string {
            auto data_type = GetNumericImGuiDataType<T>();
            std::string data_type_formatting = std::string(ImGui::DataTypeGetInfo(data_type)->ScanFmt);

            data_type_formatting = data_type_formatting.insert(1, format);
            return data_type_formatting;
        }

        template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
        static inline auto numeric_input(const char* label_ID, T &value_reference, bool can_drag, bool centerAlign, ImDrawFlags rounding_type = ImDrawFlags_RoundCornersAll, f32 rounding = GImGui->Style.FrameRounding, ImGuiInputTextFlags flags = ImGuiInputTextFlags_None) -> bool {
            ImGuiDataType data_type = GetNumericImGuiDataType<T>();
            auto value = &value_reference;

            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->SkipItems) { return false; }

            ImGuiContext& g = *GImGui;
            ImGuiStyle& style = g.Style;

            std::string formatString = GetSuitableFormat<T>();
            const char* localFormat = formatString.c_str();

            if (!can_drag) {
                std::array<char, 64> buf = {};
                ImGui::DataTypeFormatString(buf.data(), buf.size(), data_type, value, localFormat);


                flags |= ImGuiInputTextFlags_AutoSelectAll; // We call MarkItemEdited() ourselves by comparing the actual data rather than the string.

                bool value_changed = false;
                if (step != s_cast<T>(0)) {
                    const f32 button_size = ImGui::GetFrameHeight();

                    ImGui::BeginGroup(); // The only purpose of the group here is to allow the caller to query item data e.g. IsItemActive()
                    ImGui::PushID(label_ID);
                    ImGui::SetNextItemWidth(ImMax(1.0f, ImGui::CalcItemWidth() - (button_size + style.ItemInnerSpacing.x) * 2));
                    if (ImGui::InputText("", buf.data(), buf.size(), flags)) {
                        value_changed = ImGui::DataTypeApplyFromText(buf.data(), data_type, value, localFormat);
                    }
                    IMGUI_TEST_ENGINE_ITEM_INFO(g.LastItemData.ID, label, g.LastItemData.StatusFlags);

                    // Step buttons
                    const ImVec2 backup_frame_padding = style.FramePadding;
                    style.FramePadding.x = style.FramePadding.y;
                    if (flags & ImGuiInputTextFlags_ReadOnly) { ImGui::BeginDisabled(); }
                    ImGui::SameLine(0, style.ItemInnerSpacing.x);
                    if (ImGui::ButtonEx("-", ImVec2(button_size, button_size)))
                    {
                        value_reference -= g.IO.KeyCtrl ? s_cast<T>(stepFast) : s_cast<T>(step);
                        value_changed = true;
                    }
                    ImGui::SameLine(0, style.ItemInnerSpacing.x);
                    if (ImGui::ButtonEx("+", ImVec2(button_size, button_size)))
                    {
                        value_reference += g.IO.KeyCtrl ? s_cast<T>(stepFast) : s_cast<T>(step);
                        value_changed = true;
                    }
                    if (flags & ImGuiInputTextFlags_ReadOnly) { ImGui::EndDisabled(); }

                    const char* label_end = ImGui::FindRenderedTextEnd(label_ID);
                    if (label_ID != label_end)
                    {
                        ImGui::SameLine(0, style.ItemInnerSpacing.x);
                        ImGui::TextEx(label_ID, label_end);
                    }
                    style.FramePadding = backup_frame_padding;

                    ImGui::PopID();
                    ImGui::EndGroup();
                }
                else {
                    if (ImGui::InputText(label_ID, buf.data(), buf.size(), flags)) {
                        value_changed = ImGui::DataTypeApplyFromText(buf.data(), data_type, value, localFormat);
                    }
                }
                if (value_changed) {
                    ImGui::MarkItemEdited(g.LastItemData.ID);
                }

                return value_changed;
            }
            else {
                const ImGuiID id = window->GetID(label_ID);
                const f32 w = ImGui::CalcItemWidth();

                const ImVec2 label_size = ImGui::CalcTextSize(label_ID, nullptr, true);
                const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f));
                const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));

                const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
                ImGui::ItemSize(total_bb, style.FramePadding.y);
                if (!ImGui::ItemAdd(total_bb, id, &frame_bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0)) {
                    return false;
                }

                const bool hovered = ImGui::ItemHoverable(frame_bb, id, ImGuiItemFlags_None);
                bool temp_input_is_active = temp_input_allowed && ImGui::TempInputIsActive(id);
                if (!temp_input_is_active) {
                    // Tabbing or CTRL-clicking on Drag turns it into an InputText
                    const bool input_requested_by_tabbing = temp_input_allowed;
                    const bool clicked = (hovered && g.IO.MouseClicked[0]);
                    const bool double_clicked = (hovered && g.IO.MouseClickedCount[0] == 2);
                    const bool make_active = (input_requested_by_tabbing || clicked || double_clicked || g.NavActivateId == id);
                    if (make_active && temp_input_allowed) {
                        if (input_requested_by_tabbing || (clicked && g.IO.KeyCtrl) || double_clicked || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))) {
                            temp_input_is_active = true;
                        }
                    }

                    // (Optional) simple click (without moving) turns Drag into an InputText
                    if (g.IO.ConfigDragClickToInputText && temp_input_allowed && !temp_input_is_active) {
                        if (g.ActiveId == id && hovered && g.IO.MouseReleased[0] && !ImGui::IsMouseDragPastThreshold(0, g.IO.MouseDragThreshold * 0.50f)) {
                            g.NavActivateId = id;
                            g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
                            temp_input_is_active = true;
                        }
                    }

                    if (make_active && !temp_input_is_active)
                    {
                        ImGui::SetActiveID(id, window);
                        ImGui::SetFocusID(id, window);
                        ImGui::FocusWindow(window);
                        g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
                    }
                }

                if (temp_input_is_active)
                {
                    // Only clamp CTRL+Click input when ImGuiSliderFlags_AlwaysClamp is set
                    const bool is_clamp_input = (flags & ImGuiSliderFlags_AlwaysClamp) != 0 && ImGui::DataTypeCompare(data_type, &dataLimitMin, &dataLimitMax) < 0;
                    return ImGui::TempInputScalar(frame_bb, id, label_ID, data_type, value, localFormat, is_clamp_input ? &dataLimitMin : nullptr, is_clamp_input ? &dataLimitMax : nullptr);
                }

                // Draw frame
                const ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
                ImGui::RenderNavHighlight(frame_bb, id);
                ImGui::GetWindowDrawList()->AddRectFilled(frame_bb.Min, frame_bb.Max, frame_col, rounding, rounding_type);

                // Drag behavior
                const bool value_changed = ImGui::DragBehavior(id, data_type, value, dataDragSpeed, &dataLimitMin, &dataLimitMax, localFormat, flags);
                if (value_changed) { ImGui::MarkItemEdited(id); }

                // Display value using user-provided display format so user can add prefix/suffix/decorations to the value.
                std::array<char, 64> value_buf = {};
                const char* value_buf_end = value_buf.data() + ImGui::DataTypeFormatString(value_buf.data(), value_buf.size(), data_type, value, localFormat);
                if (g.LogEnabled) {
                    ImGui::LogSetNextTextDecoration("{", "}");
                }

                f32 xAlign = centerAlign ? 0.5f : g.Style.ItemInnerSpacing.x / ImGui::GetItemRectSize().x;
                f32 yAlign = centerAlign ? 0.5f : 0.0f;
                ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, value_buf.data(), value_buf_end, nullptr, ImVec2(xAlign, yAlign));

                if (label_size.x > 0.0f) {
                    ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label_ID);
                }

                IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags);
                return value_changed;
            }

        }

        auto string_property(const char* label, std::string &value, const char* tooltip, const ImGuiInputTextFlags input_flags) -> bool {
            begin_property(label);
            bool modified = string_input(IDBuffer.data(), value, input_flags);
            show_tooltip(tooltip);
            end_property();

            return modified;
        }

        auto u64_property(const char* label, u64 &value, const char* tooltip, const ImGuiInputTextFlags input_flags) -> bool {
            begin_property(label);
            bool modified = numeric_input<u64>(IDBuffer.data(), value, true, false, ImDrawFlags_RoundCornersAll, GImGui->Style.FrameRounding, input_flags);
            show_tooltip(tooltip);
            end_property();

            return modified;
        }

        auto i32_property(const char* label, i32 &value, const char* tooltip, const ImGuiInputTextFlags input_flags) -> bool {
            begin_property(label);
            bool modified = numeric_input<i32>(IDBuffer.data(), value, true, false, ImDrawFlags_RoundCornersAll, GImGui->Style.FrameRounding, input_flags);
            show_tooltip(tooltip);
            end_property();

            return modified;
        }

        auto f32_property(const char* label, f32 &value, const char* tooltip, const ImGuiInputTextFlags input_flags) -> bool {
            begin_property(label);
            bool modified = numeric_input<f32>(IDBuffer.data(), value, true, false, ImDrawFlags_RoundCornersAll, GImGui->Style.FrameRounding, input_flags);
            show_tooltip(tooltip);
            end_property();

            return modified;
        }

        auto string_input(const char* label_ID, std::string &value, const ImGuiInputTextFlags input_flags) -> bool {
            std::memset(buffer.data(), 0, sizeof(buffer));
            std::memcpy(buffer.data(), value.c_str(), sizeof(buffer));
            bool modified =  ImGui::InputText("##Tag", buffer.data(), buffer.size());
            if(modified) { value = std::string(buffer.data()); }
            return modified;
        }

        auto Vector2Input(glm::vec2 &value, const f32 *reset_values, const char** tooltips) -> bool {
            //auto boldFont = ImGui::GetIO().Fonts->Fonts[boldFontIndex];

            f32 frame_height = ImGui::GetFrameHeight();
            ImVec2 button_size = { frame_height, frame_height };

            bool drawButtons = true;
            f32 inputFieldWidth = (ImGui::GetColumnWidth(1) - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 2.0f - button_size.x;

            ImDrawFlags inputFieldRounding = ImDrawFlags_RoundCornersRight;

            if (button_size.x > 0.45f * inputFieldWidth) {
                drawButtons = false;
                inputFieldWidth += button_size.x;

                inputFieldRounding = ImDrawFlags_RoundCornersAll;
            }

            bool modified = false;

            // X
            {
                if (drawButtons) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
                    //ImGui::PushFont(boldFont);

                    if (GUI::rounded_button("X", button_size, ImDrawFlags_RoundCornersLeft)) {
                        value.x = reset_values[0];
                        modified = true;
                    }

                    //ImGui::PopFont();
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                }

                ImGui::SetNextItemWidth(inputFieldWidth);
                if (numeric_input<f32>("##X", value.x, true, true, inputFieldRounding)) {
                    modified = true;
                }

                if(tooltips != nullptr) { show_tooltip(tooltips[0]); }

                if(drawButtons) { ImGui::PopStyleVar(); }
            }

            ImGui::SameLine();

            // Y
            {
                if(drawButtons) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                    //ImGui::PushFont(boldFont);

                    if(GUI::rounded_button("Y", button_size, ImDrawFlags_RoundCornersLeft)) {
                        value.y = reset_values[1];
                        modified = true;
                    }

                    //ImGui::PopFont();
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                }

                ImGui::SetNextItemWidth(inputFieldWidth);
                if(numeric_input<f32>("##Y", value.y, true, true, inputFieldRounding)) {
                    modified = true;
                }

                if(tooltips != nullptr) { show_tooltip(tooltips[1]); }

                if (drawButtons) { ImGui::PopStyleVar(); }
            }

            ImGui::SameLine();

            // // Z
            // {
            //     if (drawButtons) {
            //         ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            //         ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            //         ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
            //         ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
            //         ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
            //         //ImGui::PushFont(boldFont);

            //         if (GUI::rounded_button("Z", button_size, ImDrawFlags_RoundCornersLeft)) {
            //             value.z = reset_values[2];
            //             modified = true;
            //         }

            //         //ImGui::PopFont();
            //         ImGui::PopStyleColor(4);

            //         ImGui::SameLine();
            //     }

            //     ImGui::SetNextItemWidth(inputFieldWidth);
            //     if (numeric_input<f32>("##Z", value.z, true, true, inputFieldRounding)) {
            //         modified = true;
            //     }

            //     if (tooltips != nullptr) { show_tooltip(tooltips[2]); }

            //     if (drawButtons) { ImGui::PopStyleVar(); }
            // }

            return modified;
        }

        auto Vector3Input(glm::vec3 &value, const f32 *reset_values, const char** tooltips) -> bool {
            //auto boldFont = ImGui::GetIO().Fonts->Fonts[boldFontIndex];

            f32 frame_height = ImGui::GetFrameHeight();
            ImVec2 button_size = { frame_height, frame_height };

            bool drawButtons = true;
            f32 inputFieldWidth = (ImGui::GetColumnWidth(1) - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f - button_size.x;

            ImDrawFlags inputFieldRounding = ImDrawFlags_RoundCornersRight;

            if (button_size.x > 0.45f * inputFieldWidth) {
                drawButtons = false;
                inputFieldWidth += button_size.x;

                inputFieldRounding = ImDrawFlags_RoundCornersAll;
            }

            bool modified = false;

            // X
            {
                if (drawButtons) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
                    //ImGui::PushFont(boldFont);

                    if (GUI::rounded_button("X", button_size, ImDrawFlags_RoundCornersLeft)) {
                        value.x = reset_values[0];
                        modified = true;
                    }

                    //ImGui::PopFont();
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                }

                ImGui::SetNextItemWidth(inputFieldWidth);
                if (numeric_input<f32>("##X", value.x, true, true, inputFieldRounding)) {
                    modified = true;
                }

                if(tooltips != nullptr) { show_tooltip(tooltips[0]); }

                if(drawButtons) { ImGui::PopStyleVar(); }
            }

            ImGui::SameLine();

            // Y
            {
                if(drawButtons) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                    //ImGui::PushFont(boldFont);

                    if(GUI::rounded_button("Y", button_size, ImDrawFlags_RoundCornersLeft)) {
                        value.y = reset_values[1];
                        modified = true;
                    }

                    //ImGui::PopFont();
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                }

                ImGui::SetNextItemWidth(inputFieldWidth);
                if(numeric_input<f32>("##Y", value.y, true, true, inputFieldRounding)) {
                    modified = true;
                }

                if(tooltips != nullptr) { show_tooltip(tooltips[1]); }

                if (drawButtons) { ImGui::PopStyleVar(); }
            }

            ImGui::SameLine();

            // Z
            {
                if (drawButtons) {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
                    //ImGui::PushFont(boldFont);

                    if (GUI::rounded_button("Z", button_size, ImDrawFlags_RoundCornersLeft)) {
                        value.z = reset_values[2];
                        modified = true;
                    }

                    //ImGui::PopFont();
                    ImGui::PopStyleColor(4);

                    ImGui::SameLine();
                }

                ImGui::SetNextItemWidth(inputFieldWidth);
                if (numeric_input<f32>("##Z", value.z, true, true, inputFieldRounding)) {
                    modified = true;
                }

                if (tooltips != nullptr) { show_tooltip(tooltips[2]); }

                if (drawButtons) { ImGui::PopStyleVar(); }
            }

            return modified;
        }

        auto vec2_property(const char* label, glm::vec2 &value, const f32 *reset_values, const char** tooltips) -> bool {
            begin_property(label);
            bool modified = Vector2Input(value, reset_values, tooltips);
            end_property();

            return modified;
        }

        auto vec3_property(const char* label, glm::vec3 &value, const f32 *reset_values, const char** tooltips) -> bool {
            begin_property(label);
            bool modified = Vector3Input(value, reset_values, tooltips);
            end_property();

            return modified;
        }
    }
}