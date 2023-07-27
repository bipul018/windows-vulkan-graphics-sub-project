struct PSInput
{
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float4 Color : COLOR0;
    [[vk::location(1)]] float4 Normal : NORMAL;
    [[vk::location(2)]] float4 model_pos : AAA;
    
};

struct PSOutput
{
    [[vk::location(0)]] float4 Color : SV_Target0;
};


struct PushConst
{
    [[vk::offset(64)]]float4 light_pos;
    float4 light_col;
};

[[vk::push_constant]]  PushConst push_data;


PSOutput main(PSInput input)
{
    
    PSOutput output;
    output.Color = input.Color;
    float prod = dot(normalize(push_data.light_pos - input.model_pos), normalize(input.Normal));
    //printf("Light : %f %f %f %f\tColor : %f %f %f %f\t", push_data.light_pos.x, push_data.light_pos.y, push_data.light_pos.z, push_data.light_pos.w, push_data.light_col.x, push_data.light_col.y, push_data.light_col.z, push_data.light_col.w);
    output.Color = mul(input.Color, prod);
    output.Color = max(float4(0.f, 0.f, 0.f, 1.f),
                        output.Color) + float4(0.1f, 0.1f, 0.1f, 0.f);
    return output;
}