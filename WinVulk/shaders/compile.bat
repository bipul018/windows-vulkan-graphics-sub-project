cd in\
for %%v in (*.vert) do (
	cd ..
	D:\prgming\VulkanSDK\1.3.231.1\Bin\glslc.exe in\%%v -o out\%%v.spv
	cd in\
)
for %%v in (*.frag) do (
	cd ..
	D:\prgming\VulkanSDK\1.3.231.1\Bin\glslc.exe in\%%v -o out\%%v.spv
	cd in\
)