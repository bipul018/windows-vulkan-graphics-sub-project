struct VSInput
{
    [[vk::location(0)]] float3 Position : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
    [[vk::location(2)]] float3 Color : COLOR0;
    [[vk::location(3)]] float2 TexCoord : TEXCOORD0;
};

struct PushConst
{
    float4x4 view_proj;
    float4 model_light[4];
};

[[vk::push_constant]] PushConst push_data;


struct VSOutput
{
    float4 Position : SV_POSITION;
    [[vk::location (0) ]] float4 Color : COLOR0;
    [[vk::location (1) ]]float4 Normal : NORMAL0;
    [[vk::location (2) ]]float4 Light : COLOR1;
};

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
    float4x4 model = float4x4(push_data.model_light[0], push_data.model_light[1], push_data.model_light[2], float4(0.f, 0.f, 0.f, 1.f));
    output.Color = float4(input.Color.xyz, 1.0);
    output.Light = push_data.model_light[3];
    output.Normal = float4(input.Normal.xyz, 0.f);
    output.Normal = normalize(mul(model, output.Normal));
    //output.Color = mul(output.Color, -dot(normalize(output.Normal), output.Light));
    output.Position = mul(push_data.view_proj,
                          mul(model,
                              float4(input.Position.xyz, 1.0)));
    
    //printf("%f,%f,%f->%f,%f,%f\n", input.Position.x, input.Position.y, input.Position.z, output.Position.x, output.Position.y, output.Position.z);    
    //printmat(push_data.view_proj);
    //printf("%f %f %f\n", output.Normal.x, output.Normal.y, output.Normal.z, output.Normal.w);
    //printf("%f %f %f\n", output.Light.x, output.Light.y, output.Light.z, output.Light.w);
    return output;
}