struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
    [[vk::location(2)]] float3 Color : COLOR0;
    [[vk::location(3)]] float2 TexCoord : TEXCOORD0;
};

struct PushConst
{
    float4x4 model;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    [[vk::location (0) ]] float4 Color : COLOR0;
    [[vk::location (1) ]] float4 Normal : NORMAL;
    [[vk::location(2)]] float4 model_pos : AAA;
};

struct UBO
{
    float4x4 view_proj;
};

[[vk::push_constant]] PushConst push_data;

ConstantBuffer<UBO> ubo_data : register(b0, space0);

void printmat(float4x4 mat)
{
    printf("___\n");
    printf("[%f %f %f %f]\n", mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
    printf("[%f %f %f %f]\n", mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
    printf("[%f %f %f %f]\n", mat[2][0], mat[2][1], mat[2][2], mat[2][3]);
    printf("[%f %f %f %f]\n", mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
    printf("___\n");
    
}

VSOutput
    main(VSInput input)
{
   
    VSOutput output = (VSOutput) 0;
    output.Color = float4(input.Color.xyz, 1.0);
    
    output.model_pos = mul(push_data.model, float4(input.Position.xyz, 1.0));
        
    output.Normal = float4(input.Normal.xyz, 0.f);
    output.Normal = normalize(mul(push_data.model, output.Normal));
    
    //output.Color = mul(output.Color, -dot(normalize(output.Normal), output.Light));
    output.Position = mul(ubo_data.view_proj, output.model_pos);
    
    //printf("%f,%f,%f->%f,%f,%f\n", input.Position.x, input.Position.y, input.Position.z, output.Position.x, output.Position.y, output.Position.z);    
    //printmat(push_data.view_proj);
    //printf("%f %f %f : %f %f %f %f\n", input.Normal.x, input.Normal.y, input.Normal.z, output.Normal.x, output.Normal.y, output.Normal.z, output.Normal.w);
    //printf("%f %f %f \n", output.Normal.x, output.Normal.y, output.Normal.z, output.Normal.w);
    //printf("%f %f %f\n", output.Light.x, output.Light.y, output.Light.z, output.Light.w);
    return output;
}