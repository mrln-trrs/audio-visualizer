# -*- coding: utf-8 -*-
# Regenera las listas de fuentes del .vcxproj, del .filters y del CMakeLists.txt a partir de src/,
# shaders/ e imgui-1.91.5/. Ejecutar desde cualquier directorio tras añadir o quitar archivos:
#     python tools/update_build_lists.py
# Los objetivos de prueba de CMake (tests/) no se tocan; se mantienen a mano en CMakeLists.txt.
import os, re, glob, uuid

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

src_cpp = sorted(p.replace('/', '\\') for p in glob.glob('src/**/*.cpp', recursive=True))
src_h = sorted(p.replace('/', '\\') for p in glob.glob('src/**/*.h', recursive=True))
imgui_cpp = [r'imgui-1.91.5\imgui.cpp', r'imgui-1.91.5\imgui_draw.cpp', r'imgui-1.91.5\imgui_tables.cpp',
             r'imgui-1.91.5\imgui_widgets.cpp', r'imgui-1.91.5\backends\imgui_impl_glfw.cpp',
             r'imgui-1.91.5\backends\imgui_impl_opengl3.cpp']
imgui_h = [r'imgui-1.91.5\imconfig.h', r'imgui-1.91.5\imgui.h', r'imgui-1.91.5\imgui_internal.h',
           r'imgui-1.91.5\backends\imgui_impl_glfw.h', r'imgui-1.91.5\backends\imgui_impl_opengl3.h']
shaders = sorted(p.replace('/', '\\') for p in glob.glob('shaders/*'))
none_items = ['.gitignore', 'config.json', 'README.md', 'CMakeLists.txt'] + shaders
print(len(src_cpp), 'cpp,', len(src_h), 'h en src/')

# ---------------------------------------------------------------- .vcxproj
p = 'audio-visualizer.vcxproj'
s = open(p, encoding='utf-8-sig').read()
nl = '\r\n' if '\r\n' in s else '\n'
old_inc = '<AdditionalIncludeDirectories>$(ProjectDir)glfw-3.4.bin.WIN64\\include;'
if s.count(old_inc) == 2:  # idempotente: solo la primera vez
    s = s.replace(old_inc, '<AdditionalIncludeDirectories>$(ProjectDir)src;$(ProjectDir)glfw-3.4.bin.WIN64\\include;')
assert s.count('$(ProjectDir)src;') == 2

def group(tag, items):
    return '  <ItemGroup>' + nl + ''.join('    <%s Include="%s" />' % (tag, i) + nl for i in items) + '  </ItemGroup>' + nl

new_groups = group('ClCompile', src_cpp + imgui_cpp) + group('ClInclude', src_h + imgui_h) + group('None', none_items)
pattern = re.compile(r'(  <ItemGroup>\s*<(?:ClCompile|ClInclude|None) Include=.*?</ItemGroup>\r?\n)', re.S)
groups = pattern.findall(s)
assert len(groups) == 3, len(groups)
first = s.find(groups[0])
last_end = s.find(groups[-1]) + len(groups[-1])
s = s[:first] + new_groups + s[last_end:]
open(p, 'w', encoding='utf-8', newline='').write(s)
print('vcxproj OK')

# ---------------------------------------------------------------- .filters (regenerado completo)
def filt_name(path):
    d = os.path.dirname(path)
    if d.startswith('src'):
        rel = d[4:]
        return 'Fuentes' + ('\\' + rel if rel else '')
    if d.startswith('imgui-1.91.5'):
        return 'Terceros\\imgui'
    if d.startswith('shaders'):
        return 'Shaders'
    return ''

all_files = ([(f, 'ClCompile') for f in src_cpp + imgui_cpp] + [(f, 'ClInclude') for f in src_h + imgui_h] +
             [(f, 'None') for f in none_items])
filters = set()
for f, _ in all_files:
    n = filt_name(f)
    while n:
        filters.add(n)
        n = n.rsplit('\\', 1)[0] if '\\' in n else ''
filters.add('Terceros')
out = ['<?xml version="1.0" encoding="utf-8"?>',
       '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">',
       '  <ItemGroup>']
for n in sorted(filters):
    out.append('    <Filter Include="%s">' % n)
    out.append('      <UniqueIdentifier>{%s}</UniqueIdentifier>' % uuid.uuid5(uuid.NAMESPACE_URL, 'audio-visualizer/' + n))
    out.append('    </Filter>')
out.append('  </ItemGroup>')
for tag in ('ClCompile', 'ClInclude', 'None'):
    out.append('  <ItemGroup>')
    for f, t in all_files:
        if t != tag:
            continue
        n = filt_name(f)
        if n:
            out.append('    <%s Include="%s">' % (tag, f))
            out.append('      <Filter>%s</Filter>' % n)
            out.append('    </%s>' % tag)
        else:
            out.append('    <%s Include="%s" />' % (tag, f))
    out.append('  </ItemGroup>')
out.append('</Project>')
open('audio-visualizer.vcxproj.filters', 'w', encoding='utf-8-sig', newline='').write('\r\n'.join(out) + '\r\n')
print('filters OK')

# ---------------------------------------------------------------- CMakeLists.txt
p = 'CMakeLists.txt'
s = open(p, encoding='utf-8').read()
start = s.find('set(PROJECT_SOURCES')
end = s.find(')', start) + 1
assert start > 0
cm_sources = 'set(PROJECT_SOURCES\n' + ''.join('    %s\n' % f.replace('\\', '/') for f in src_cpp + src_h) + ')'
s = s[:start] + cm_sources + s[end:]
old = 'target_include_directories(audio-visualizer PRIVATE\n    ${CMAKE_CURRENT_SOURCE_DIR}\n'
if old in s:  # idempotente: solo la primera vez
    s = s.replace(old, 'target_include_directories(audio-visualizer PRIVATE\n    ${CMAKE_CURRENT_SOURCE_DIR}/src\n')
assert '${CMAKE_CURRENT_SOURCE_DIR}/src\n' in s
if 'source_group' not in s:
    s = s.replace('# Aplicación de ventana', 'source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Fuentes" FILES ${PROJECT_SOURCES})\n\n# Aplicación de ventana')
open(p, 'w', encoding='utf-8', newline='').write(s)
print('CMakeLists OK')
