// Need to add more here

#include <cstdint>
#include <cmath>



struct int10f11f11f_rev {

    int32_t     x:11;
    int32_t     y:11;
    int32_t     z:10;

    static int10f11f11f_rev pack(const float x, const float y, const float z) {
        int10f11f11f_rev value;
        value.x = x * 1023.0f;
        value.y = y * 1023.0f;
        value.z = z * 511.0f;
        return value;
    }

    static void unpack(
            const int10f11f11f_rev value,
            float& x,
            float& y,
            float& z
    ) {
        x = float(value.x) * (1.0f / 1023.0f);
        y = float(value.y) * (1.0f / 1023.0f);
        z = float(value.z) * (1.0f / 511.0f);
    }

};

