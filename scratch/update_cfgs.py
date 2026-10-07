import re

cfg_path = 'F:/Allclient/cstrike/config.cfg'
try:
    with open(cfg_path, 'r', encoding='latin1') as f:
        cfg = f.read()
    if '_vgui_menus' not in cfg:
        with open(cfg_path, 'a', encoding='latin1') as f:
            f.write('\nsetinfo "_vgui_menus" "0"\n')
        print('Added _vgui_menus to config.cfg')
    else:
        print('_vgui_menus already in config.cfg')
except Exception as e:
    print('config.cfg error:', e)

xml_path = 'F:/Allclient/platform/steam/games/SmartEmu/config.xml'
try:
    with open(xml_path, 'r', encoding='utf-8') as f:
        xml = f.read()
    xml_new = re.sub(r'<CommandLine>.*?</CommandLine>', '<CommandLine>-steam -gl -fullscreen</CommandLine>', xml)
    with open(xml_path, 'w', encoding='utf-8') as f:
        f.write(xml_new)
    print('Updated SmartEmu config.xml')
except Exception as e:
    print('config.xml error:', e)
