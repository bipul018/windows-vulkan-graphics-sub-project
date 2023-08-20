# windows-vulkan-graphics-sub-project
This is a pratice project for windows (win32) - vulkan and for graphics project of 5th semester.


Dependencies required:

Windows OS 

header only dependency : stb_truetype : https://github.com/nothings/stb.git  

Full library dependency : Vulkan SDK : https://vulkan.lunarg.com/sdk/home#windows

To setup vulkan, install the sdk from above link .
Also setup the IDE to point at header file location of stb_truetype.h and vulkan headers to run.
If you have Visual Studio, then just open the .sln file. Then setup the Vulkan library directory, Vulkan headers director, and stb_truetype.h directory in the project properties.

Screenshots:
![meshview](https://github.com/bipul018/windows-vulkan-graphics-sub-project/assets/83596423/e835b554-32a1-489f-a3c2-2a6155e47481)

![thanks](https://github.com/bipul018/windows-vulkan-graphics-sub-project/assets/83596423/7f65a430-4315-4bab-97e9-fd1161f00f2e)

Triangulation Comparision of Blender's and mine:

Blender's Triangulation:
![blender triangulation](https://github.com/bipul018/windows-vulkan-graphics-sub-project/assets/83596423/e5295f19-ddab-44f6-bd36-96a2cd989edb)


Mine :
![my triangulation](https://github.com/bipul018/windows-vulkan-graphics-sub-project/assets/83596423/f2993a3c-8e53-46f3-b947-5a8d0b8e79bc)



Running the exe:

To run the exe after compilation make sure to have the following at the same folder

1> Any font file {.ttf} at the WinVulk folder

2> shaders folder

A sample .exe is situated at \WinVulk\WinVulk.exe

This .exe may not be updated, if needed so then compile the source code to full

Controls:
The GUI works as shown, click on the list of letters to load a letter onto the screen. 
ADWSQE => controls the x y and z of translation
JLIKUO => controls the x y and z of rotation
Scroll Vertical => controls the scaling
