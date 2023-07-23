struct PSInput
{
    [[vk::location(0)]] float4 Color : VECTOR1;
    [[vk::location(1)]] float4 Normal : NORMAL0;
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
    output.Color = mul(input.Color, -dot(input.Light, normalize(input.Normal)));
    output.Color = max(float4(0.f, 0.f, 0.f, 1.f),
                        output.Color) + float4(0.f, 0.0f, 0.0f, 0.f);
    return output;
}