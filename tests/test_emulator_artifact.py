"""In-memory malformed headers are parser fixtures, not console artifacts."""
import struct,unittest
from unittest.mock import patch
from tools.emulator_artifact import checked_resources
class ArtifactMetadata(unittest.TestCase):
    def blob(self,payload=b'resource-data'):
        header=bytearray(44);header[:4]=b'3DSX';struct.pack_into('<HHII',header,4,44,8,0,0);struct.pack_into('<III',header,32,44,4,48)
        return bytes(header)+b'SMDH'+payload
    def test_exact_source_and_unique_embedded_metadata(self):
        with patch('tools.emulator_artifact.subprocess.check_output',return_value=b'resource-data')as read:
            self.assertEqual(checked_resources(self.blob(),'a'*40,['romfs/world/fixture.encroom'],'.'),{'romfs/world/fixture.encroom':b'resource-data'})
            read.assert_called_once_with(['git','show','a'*40+':romfs/world/fixture.encroom'],cwd='.')
    def test_unknown_source_paths_and_missing_duplicate_data_rejected(self):
        for source in ('main','f'*39,'g'*40,None,True,1):
            with self.assertRaises(ValueError):checked_resources(self.blob(),source,['romfs/world/x'],'.')
        for paths in ([],['../x'],['romfs/../x'],['romfs//x'],['romfs/x:y'],['romfs/x','romfs/x']):
            with patch('tools.emulator_artifact.subprocess.check_output',return_value=b'resource-data'),self.assertRaises(ValueError):checked_resources(self.blob(),'a'*40,paths,'.')
        for payload in (b'wrong',b'resource-dataresource-data'):
            with patch('tools.emulator_artifact.subprocess.check_output',return_value=b'resource-data'),self.assertRaises(ValueError):checked_resources(self.blob(payload),'a'*40,['romfs/world/x'],'.')
        for blob in (b'',b'wrong-header',self.blob()[:47]):
            with self.assertRaises(ValueError):checked_resources(blob,'a'*40,['romfs/world/x'],'.')
if __name__=='__main__':unittest.main()
