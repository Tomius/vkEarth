#version 400

layout(constant_id = 17) const int arraySize = 12;
in vec4 ucol[arraySize];

layout(constant_id = 17) const bool spBool = true;
layout(constant_id = 18) const float spFloat = 3.14;
layout(constant_id = 19) const double spDouble = 3.1415926535897932384626433832795;
layout(constant_id = 22) const uint scale = 2;

out vec4 color;
out int size;

void main()
{
    color = ucol[2];
    size = arraySize;
    if (spBool)
        color *= scale;
    color += float(spDouble / spFloat);
}
