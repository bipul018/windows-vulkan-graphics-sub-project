struct PSInput
{
    float4 Position : SV_POSITION;
    [[vk::location(0)]] float4 Normal : NORMAL;
    [[vk::location(1)]] float4 world_pos : AAA;
    [[vk::location(2)]] float2 TexCoord : TEXCOORD0;
};

struct PSOutput
{
    [[vk::location(0)]] float4 Color : SV_Target0;
};

struct PushConst
{
    [[vk::offset(64)]]float4 model_color;
};

struct LightSource
{
    float4 source_location;
    float4 source_color;
};

[[vk::push_constant]] PushConst push_data;

ConstantBuffer<LightSource> ubo_data : register(b0, space1);

PSOutput main(PSInput input)
{
    
    PSOutput output;
    float4 diff = ubo_data.source_location - input.world_pos;
    float len = length(diff);
    diff = diff / (len*len*0.001  + len  );
    //printf("%f ", len);
    
    float prod = dot(diff,
                     normalize(input.Normal));
    
    
    output.Color = mul(ubo_data.source_color, prod);
    
    output.Color = max(float4(0.f, 0.f, 0.f, 1.f),
                        output.Color) + float4(0.4f, 0.1f, 0.1f, 0.f);
    
    //printf("Light : %f %f %f %f\tColor : %f %f %f %f\t", ubo_data.source_location.x, ubo_data.source_location.y, ubo_data.source_location.z, ubo_data.source_location.w, ubo_data.source_color.x, ubo_data.source_color.y, ubo_data.source_color.z, ubo_data.source_color.w);
    //printf("Model : %f %f %f %f\tColor : %f %f %f %f\t", push_data.model_color.x, push_data.model_color.y, push_data.model_color.z, push_data.model_color.w, output.Color.x, output.Color.y, output.Color.z, output.Color.w);
    output.Color = output.Color * push_data.model_color;
    
    return output;
}