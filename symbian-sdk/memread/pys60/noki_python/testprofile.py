import time
import sys
import e32
import appuifw
import thread
import profileengine

# create profileengine instance
pe = profileengine.GetProfileEngine();

# ID of current profile
print "Active profile ID = ", pe.ActiveProfileId();

# available profiles
pna = pe.ProfileNameArray();
index = 0;
count = pna.Count();
while (index < count):
    print pna.ProfileId(index), pna.ProfileName(index);
    index = index + 1;
# release profile array    
pna.Release();    

# use predefined ID (see profileengine.py) to set to silent
print "Setting to silent"
pe.SetActiveProfile(profileengine.EProfileSilentId);

# ID of current profile
print "Active profile ID = ", pe.ActiveProfileId();

pe.Release();
print 'Done';
