import time
import sys
import e32
import appuifw
import thread
import wlanmgmt

# create WLANMgmtClient instance
mc = wlanmgmt.CreateWLANMgmtClient();

#do scan
scanResults = mc.GetScanResults();
scanResults.First();

while (not scanResults.IsDone()):
	print scanResults.Bssid();
	print scanResults.Ssid();
	print scanResults.RXLevel();
	print "---"
	scanResults.Next();


scanResults.Release();

mc.Release();
print 'Done';
