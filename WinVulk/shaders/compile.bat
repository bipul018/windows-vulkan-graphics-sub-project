cd in\
for %%f in (*.frag.hlsl) do (
	cd ..
	D:\prgming\VulkanSDK\1.3.231.1\Bin\dxc.exe -spirv -T ps_6_0 -E main .\in\%%f -Fo .\out\%%f.spv
	cd in\
)
for %%v in (*.vert.hlsl) do (
	cd ..
	D:\prgming\VulkanSDK\1.3.231.1\Bin\dxc.exe -spirv -T vs_6_0 -E main .\in\%%v -Fo .\out\%%v.spv
	cd in\
)
