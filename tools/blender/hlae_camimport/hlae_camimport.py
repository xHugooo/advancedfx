#!BPY

"""
Name: 'HLAE camera motion (.bvh)'
Blender: 246
Group: 'Import'
Tip: 'Import HLAE camera motion data'
"""

__author__ = "ripieces"
__url__ = "advancedfx.org"
__version__ = "0.0.0.4 (2009-09-04T14:44Z)"

__bpydoc__ = """\
HLAE camera motion Import

For more info see http://www.advancedfx.org/
"""


# Copyright (c) advancedfx.org
#
# Last changes:
# 2009-09-04 by dominik.matrixstorm.com
#
# First changes:
# 2009-09-01 by dominik.matrixstorm.com


# 0.01745329251994329576923690768488...
DEG2RAD = 0.01745329252

# 3.141592653589793238462643383279...
PIE = 3.14159265358979323846264338328


import Blender


def SetError(error):
	print 'ERROR:', error
	Blender.Draw.PupBlock('Error!', [(error)])
	

# <summary> reads a line from file and separates it into words by splitting whitespace </summary>
# <param name="file"> file to read from </param>
# <returns> list of words </returns>
def ReadLineWords(file):
	line = file.readline()
	words = [ll for ll in line.split() if ll]	
	return words


# <summary> searches a list of words for a word by lower case comparison </summary>
# <param name="words"> list to search </param>
# <param name="word"> word to find </param>
# <returns> less than 0 if not found, otherwise the first list index </returns>
def FindWordL(words, word):
	i = 0
	word = word.lower()
	while i < len(words):
		if words[i].lower() == word:
			return i
		i += 1
	return -1


# <summary> Scans the file till a line containing a lower case match of filterWord is found </summary>
# <param name="file"> file to read from </param>
# <param name="filterWord"> word to find </param>
# <returns> False on fail, otherwise same as ReadLineWords for this line </returns>
def ReadLineWordsFilterL(file, filterWord):
	while True:
		words = ReadLineWords(file)
		if 0 < len(words):
			if 0 <= FindWordL(words, filterWord):
				return words
		else:
			return False


# <summary> Scans the file till the channels line and reads channel information </summary>
# <param name="file"> file to read from </param>
# <returns> False on fail, otherwise channel indexes as follows: [Xposition, Yposition, Zposition, Zrotation, Xrotation, Yrotation] </returns>
def ReadChannels(file):
	words = ReadLineWordsFilterL(file, 'CHANNELS')
	
	if not words:
		return False

	channels = [\
	FindWordL(words, 'Xposition'),\
	FindWordL(words, 'Yposition'),\
	FindWordL(words, 'Zposition'),\
	FindWordL(words, 'Zrotation'),\
	FindWordL(words, 'Xrotation'),\
	FindWordL(words, 'Yrotation')\
	]
	
	idx = 0
	while idx < 6:
		channels[idx] -= 2
		idx += 1
			
	for channel in channels:
		if not (0 <= channel and channel < 6):
			return False
			
	return channels
	

def ReadRootName(file):
	words = ReadLineWordsFilterL(file, 'ROOT')
	
	if not words or len(words)<2:
		return False
		
	return words[1]


def ReadFrame(file, channels):
	line = ReadLineWords(file)
	
	if len(line) < 6:
		return False;
	
	Xpos = float(line[channels[0]])
	Ypos = float(line[channels[1]])
	Zpos = float(line[channels[2]])
	Zrot = float(line[channels[3]])
	Xrot = float(line[channels[4]])
	Yrot = float(line[channels[5]])
	
	return [Xpos, Ypos, Zpos, Zrot, Xrot, Yrot]
	

def ReadFile(fileName, scale, camFov, rotFix):
	file = open(fileName, 'rU')
	
	rootName = ReadRootName(file)
	if not rootName:
		SetError('Failed parsing ROOT.')
		return False
		
	print 'ROOT:', rootName

	channels = ReadChannels(file)
	if not channels:
		SetError('Failed parsing CHANNELS.')
		return False
		
	# seek to last line before Frame data:
	if not ReadLineWordsFilterL(file, 'Time:'):
		SetError('Could not locate "Frame Time:" entry.')
		return False

	# build the IPO curves:
	ipo = Blender.Ipo.New('Object', rootName+'_Ipo')
	CrvLocX = ipo.addCurve('LocX')
	CrvLocY = ipo.addCurve('LocY')
	CrvLocZ = ipo.addCurve('LocZ')
	CrvRotX = ipo.addCurve('RotX')
	CrvRotY = ipo.addCurve('RotY')
	CrvRotZ = ipo.addCurve('RotZ')
	
	frameCount = 0
			
	while True:
		frame = ReadFrame(file, channels)
		if not frame:
			break;
			
		frameCount += 1
		
		BTT = float(frameCount)

		BXP =  frame[0] *scale # Bvh_XP
		BYP = -frame[2] *scale # Bvh_ZP
		BZP =  frame[1] *scale # Bvh_YP 

		BYR = -frame[3] # Bvh_ZR
		BXR =  frame[4] # Bvh_XR
		BZR =  frame[5] # Bvh_YR
		
		if rotFix:
			BT = BXP
			BXP = BYP
			BYP = -BT
			BZR -= 90.0
		
		BXR += 90.0 #  fix blender camera to point up at z

		BXP = float(BXP)
		BYP = float(BYP)
		BZP = float(BZP)
		BXR = float(BXR) / 10.0
		BYR = float(BYR) / 10.0
		BZR = float(BZR) / 10.0
		
		CrvLocX.append((BTT, BXP))
		CrvLocY.append((BTT, BYP))
		CrvLocZ.append((BTT, BZP))

		CrvRotX.append((BTT, BXR))
		CrvRotY.append((BTT, BYR))
		CrvRotZ.append((BTT, BZR))
		
	# setup scene
		
	scn = Blender.Scene.GetCurrent()
	scn.objects.selected = []
	
	cam = Blender.Camera.New('persp', rootName);
	cam.angle = camFov;
	
	obCam = scn.objects.new(cam)

	obCam.setIpo(ipo)
	
	#
	
	scn.update(1)
	Blender.Window.RedrawAll()
	
	return True


def load_HlaeCamMotion(fileName):
	UI_Scale = Blender.Draw.Create(0.01)
	UI_Fov = Blender.Draw.Create(90.0)
	UI_Fix = Blender.Draw.Create(True)

	UI_block = []	
	UI_block.append(("Scale:", UI_Scale, 0.001, 10.0, 'Scaling'))
	UI_block.append(("FOV:", UI_Fov, 10.0, 170.0, 'Field of view to set for the camera (in degrees)'))
	UI_block.append(("-90deg fix", UI_Fix, 'Matches wrong BSP viewer map rotation'))
	
	if not Blender.Draw.PupBlock('HLAE camera motion import', UI_block):
		return
			
	IMP_Scale = UI_Scale.val
	IMP_Fov = UI_Fov.val
	IMP_Fix = UI_Fix.val

	print 'Importing', fileName, 'Scale =', IMP_Scale, 'FOV =', IMP_Fov, 'Fix =', IMP_Fix
	
	if ReadFile(fileName, IMP_Scale, IMP_Fov, IMP_Fix):
		print 'Done.'
	else:
		print 'FAILED';

	
def main():
	Blender.Window.FileSelector(load_HlaeCamMotion, 'Import', '*.bvh')

	
if __name__ == '__main__':
	main()