import re

file_path = r'c:\Users\rea\source\repos\2025\Chaso\Chaso\project\Application\Editor\EditorManager.cpp'
with open(file_path, 'r', encoding='utf-8') as f:
    content = f.read()

# Add pendingRemove
if 'std::function<void()> pendingRemove;' not in content:
    content = content.replace('ImGui::Text("Entity: %s", e->Name().c_str());', 'std::function<void()> pendingRemove;\n        ImGui::Text("Entity: %s", e->Name().c_str());')

# Regex to match if (auto* comp = e->GetComponent<TYPE>()) { \n if (ImGui::CollapsingHeader("TITLE", ...)) {
pattern = re.compile(r'if\s*\(\s*auto\*\s*([a-zA-Z0-9_]+)\s*=\s*e->GetComponent<([a-zA-Z0-9_]+)>\(\)\)\s*\{\s*if\s*\(\s*ImGui::CollapsingHeader\(\s*"([^"]+)"\s*,\s*ImGuiTreeNodeFlags_DefaultOpen\s*\)\s*\)\s*\{')

def repl(m):
    comp_var = m.group(1)
    comp_type = m.group(2)
    title = m.group(3)
    
    # Skip Transform
    if comp_type == 'TransformComponent':
        return m.group(0)
        
    return f'''if (auto* {comp_var} = e->GetComponent<{comp_type}>()) {{
            bool headerOpen = ImGui::CollapsingHeader("{title}", ImGuiTreeNodeFlags_DefaultOpen);
            if (ImGui::BeginPopupContextItem()) {{
                if (ImGui::MenuItem("Remove Component")) pendingRemove = [e](){{ e->RemoveComponent<{comp_type}>(); }};
                ImGui::EndPopup();
            }}
            if (headerOpen) {{'''

new_content = pattern.sub(repl, content)

# Add if (pendingRemove) pendingRemove(); at the end of the selectedEntity_ block
# We need to find the end of the e block. It ends with:
#               ImGui::EndPopup();
#           }
#       } else {
#           ImGui::Text("No entity selected");
#       }
# We can just replace "} else {" with "if (pendingRemove) pendingRemove();\n      } else {"

if 'if (pendingRemove) pendingRemove();' not in new_content:
    new_content = new_content.replace('} else {\n        ImGui::Text("No entity selected");', 'if (pendingRemove) pendingRemove();\n      } else {\n        ImGui::Text("No entity selected");')

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(new_content)

print("Processed EditorManager.cpp")
