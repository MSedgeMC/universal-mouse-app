import importlib.util
import pathlib
import unittest
spec=importlib.util.spec_from_file_location('gm320_worker',pathlib.Path(__file__).with_name('worker.py'))
w=importlib.util.module_from_spec(spec)
spec.loader.exec_module(w)

# Actual initial GM320 configuration bytes, observed without any commit.
GM320={0x13:0x10,0x15:0x78,0x16:0xde,0x17:4,0x18:9,0x19:2,
       0x1a:1,0x1b:0,0x1c:0xf7,0x1d:0xff,0x1e:0,0x33:0x3f,
       0x3a:0xb,0x3e:0x8f,0x3f:0,0x44:0x21,0x45:0x22,
       0x46:0x23,0x47:0x24,0x48:0x25,0x49:0x35,0x4a:0}
GM320={**{a:0 for a in w.MASKS},**GM320}
class GM320Tests(unittest.TestCase):
    def test_existing_gm320_disabled_slot_does_not_prevent_dpi_and_polling_reads(self):
        s=w.decode(GM320)
        self.assertEqual(s['dpi'],[1400,1600,3200,4000,5600,800])
        self.assertEqual(s['enabled'],[False,True,False,False,False,False])
        self.assertEqual(s['stage'],1)
        self.assertEqual(s['polling'],1000)
        self.assertEqual(s['button7'],'Disabled')
    def test_polling_edit_preserves_disabled_slot_and_all_other_gm320_settings(self):
        after=w.merge(GM320,{'polling':500})
        self.assertEqual([a for a in GM320 if GM320[a]!=after[a]],[0x1e])
        self.assertEqual(w.decode(after)['button7'],'Disabled')
    def test_remove_last_left_click_remains_forbidden(self):
        with self.assertRaises(ValueError):w.merge(GM320,{'button1':'Disabled'})
