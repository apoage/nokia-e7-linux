import time
import sys
import e32
import appuifw
import thread
import landmarks

# create landmarksdatabase instance
ld = landmarks.OpenDefaultDatabase();

#categoryManager = ld.CreateCategoryManager();
#print "ld ref count =",sys.getrefcount(ld)

# iterate through categories
#print 'Listing all known categories'
#categoryIter = categoryManager.CategoryIterator(landmarks.ECategorySortOrderNone);
#id = categoryIter.Next();
#while id != landmarks.KPosLmNullItemId:
#	category = categoryManager.ReadCategory(id);
#	print category.CategoryId(), category.GetCategoryName(), 'Global: ', category.GlobalCategory();
#	id = categoryIter.Next();
#	category.Close();
#
#categoryIter.Close();
#categoryManager.Close();

#remove all landmarks (be careful!!!!!!!!!)
#operation = ld.RemoveAllLandmarks();
#print 'Execute', operation.Execute();
#operation.Close();

# find landmark
#tsc = landmarks.CreateTextCriteria(u'Dummy landmark', landmarks.ELandmarkName, []);
#search = ld.CreateSearch();
#operation = search.StartLandmarkSearch(tsc, landmarks.ENoAttribute, 0, 0);
#print 'Execute search', operation.Execute();
#operation.Close();

#lmid = 0;

#if search.NumOfMatches() == 0:
#	# add landmark
#	landmark = landmarks.CreateLandmark();
#	landmark.SetLandmarkName(u'Dummy landmark');
#	lmid = ld.AddLandmark(landmark);
#	print 'Created landmark, Id=', lmid
#	landmark.Close();
#else:
#    # landmark found
#    lmid = search.MatchIterator().Next();
#    print 'Dummy landmark already exists, Id = ', lmid
#
#search.Close();
#tsc.Close();

# iterate through landmarks
#print 'Listing all known landmarks'
#landmarkIter = ld.LandmarkIterator(landmarks.ENoAttribute, 0);
#id = landmarkIter.Next();
#while id != landmarks.KPosLmNullItemId:
#	landmark = ld.ReadLandmark(id);
#	print 'Id: ', landmark.LandmarkId();
#	print 'Name: ', landmark.GetLandmarkName();
#	print 'Position: ', landmark.GetPosition();
#	id = landmarkIter.Next();
#	landmark.Close();
#
#landmarkIter.Close();
#
#print 'Loading, Id = ', lmid
#landmark = ld.ReadLandmark(lmid);
#landmark.SetPosition(1,2,3,4,5);
#ld.UpdateLandmark(landmark);
#landmark.Close();

# iterate through landmarks
#print 'Listing all known landmarks'
#landmarkIter = ld.LandmarkIterator(landmarks.ENoAttribute, 0);
#id = landmarkIter.Next();
#while id != landmarks.KPosLmNullItemId:
#	landmark = ld.ReadLandmark(id);
#	print 'Id: ', landmark.LandmarkId();
#	print 'Name: ', landmark.GetLandmarkName();
#	print 'Position: ', landmark.GetPosition();
#	id = landmarkIter.Next();
#	landmark.Close();
#
#landmarkIter.Close();

#select category
#selectedCategory = landmarks.ShowSelectMultipleCategoryDialog(1, []);
#print 'CategoryDialog: ', selectedCategory;
#print "selectedCategory ref count =",sys.getrefcount(selectedCategory)

#selectedLandmark = landmarks.ShowSelectLandmarkDialog(10);
#print 'LandmarkDialog: ', selectedLandmark;

#edit landmark
#print 'Edit landmark: ', ld.ShowEditDialog(lmid, landmarks.ELmkAll, landmarks.ELmkEditor, 0);

dm = landmarks.CreateDatabaseManager();
dbInfos = dm.ListDatabaseInfos('');
print 'Databases', dbInfos;
print 'Uri', dbInfos[0].DatabaseUri();
print 'Protocol', dbInfos[0].Protocol();
print 'Media', dbInfos[0].DatabaseMedia();
print 'Drive', dbInfos[0].DatabaseDrive();
print 'Size', dbInfos[0].Size();
settings = dbInfos[0].Settings();
print 'Name', settings.DatabaseName();
print 'IsAttributeSet', settings.IsAttributeSet(landmarks.EName);
dm.Close();

ld.Close();

# MUST call this at exit
landmarks.ReleaseLandmarkResources();
print 'Done';
