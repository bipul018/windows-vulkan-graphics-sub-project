struct PSInput
{
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float4 Color : COLOR0;
    [[vk::location(1)]] float4 Normal : NORMAL;
    [[vk::location (2) ]]float4 Light : COLOR1;
    
};

struct PSOutput
{
    [[vk::location(0)]] float4 Color : SV_Target0;
};

PSOutput main(PSInput input)
{
    PSOutput output;
    output.Color = input.Color;
    float prod = -dot(input.Light, normalize(input.Normal));
    
    output.Color = mul(input.Color, prod);
    //printf("%f\t", prod);
    output.Color = max(float4(0.f, 0.f, 0.f, 1.f),
                        output.Color) + float4(0.1f, 0.1f, 0.1f, 0.f);
    return output;
}