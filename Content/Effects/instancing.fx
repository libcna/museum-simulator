// SPDX-License-Identifier: MS-PL
// A minimal XNA Effect Framework shader for the museum's hardware-instancing rooms.
// The instance stream is four Vector4 values (one row of the world matrix each),
// matching the VertexBufferBinding declaration used by GeometryLab and PerfLab.

float4x4 WorldViewProjection;
float4 DiffuseColor;

struct VertexInput
{
    float4 Position : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 InstanceRow0 : BLENDWEIGHT0;
    float4 InstanceRow1 : BLENDWEIGHT1;
    float4 InstanceRow2 : BLENDWEIGHT2;
    float4 InstanceRow3 : BLENDWEIGHT3;
};

struct VertexOutput
{
    float4 Position : POSITION0;
    float4 Color : COLOR0;
};

VertexOutput VS(VertexInput input)
{
    VertexOutput output;
    float4x4 instanceWorld = float4x4(input.InstanceRow0,
                                      input.InstanceRow1,
                                      input.InstanceRow2,
                                      input.InstanceRow3);
    output.Position = mul(mul(input.Position, instanceWorld), WorldViewProjection);
    output.Color = DiffuseColor;
    return output;
}

float4 PS(VertexOutput input) : COLOR0
{
    return input.Color;
}

technique Instancing
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS();
        PixelShader = compile ps_2_0 PS();
    }
}
