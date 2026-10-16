# Translations of the wizard and of the export report: every key used by the wizard, the SMESH
# plugins menu and the report is in CartesianGridPlugin_msg_en.qm and _fr.qm, the English text of
# the report keys is the one of REPORT_FORMATS, and both languages use the same placeholders
import os
import re
import salome
salome.standalone()
salome.salome_init()
from qtsalome import QCoreApplication, QTranslator
from salome.CartesianGridPlugin import CartesianGridPluginWizard as Wizard

root = os.environ["CARTESIANGRIDPLUGIN_ROOT_DIR"]
app = QCoreApplication.instance() or QCoreApplication([])
CONTEXT = "CartesianGridPluginWizard"

translators = {}
for lang in ("en", "fr"):
    t = QTranslator()
    assert t.load(os.path.join(root, "share", "salome", "resources", "cartesiangridplugin",
                               "CartesianGridPlugin_msg_%s.qm" % lang)), lang
    translators[lang] = t

def placeholders(text):
    return sorted(re.findall(r"\{\d+\}|%s|%p", text))

# report: English as REPORT_FORMATS, French with the same placeholders
for key, (level, english) in Wizard.REPORT_FORMATS.items():
    assert level in ("info", "note", "warning"), (key, level)
    en = translators["en"].translate(CONTEXT, "CARTESIANGRID_REPORT_" + key)
    fr = translators["fr"].translate(CONTEXT, "CARTESIANGRID_REPORT_" + key)
    assert en == english, (key, en, english)
    assert fr and fr != en, (key, fr)
    assert placeholders(fr) == placeholders(en), (key, fr, en)

# wizard and menu keys
sources = [open(Wizard.__file__, encoding="utf-8").read(),
           open(os.path.join(root, "share", "salome", "plugins", "cartesiangridplugin", "smesh_plugins.py"),
                encoding="utf-8").read()]
keys = sorted(set(re.findall(r'"(CARTESIANGRID_WIZ_\w+)"', "".join(sources))))
assert len(keys) > 30, keys
for key in keys:
    en = translators["en"].translate(CONTEXT, key)
    fr = translators["fr"].translate(CONTEXT, key)
    assert en and fr, (key, en, fr)
    assert placeholders(fr) == placeholders(en), (key, fr, en)
print("translations OK: %d report keys, %d wizard keys" % (len(Wizard.REPORT_FORMATS), len(keys)))
