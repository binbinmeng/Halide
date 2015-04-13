#include "Halide.h"
#include <stdio.h>

using namespace Halide;

int main(int argc, char **argv) {
    if (!get_jit_target_from_environment().has_gpu_feature()) {
        printf("No gpu target enabled. Skipping test.\n");
        return 0;
    }

    Var x, y, z, w;
    Image<int> full(80, 60, 10, 10);

    const int x_off = 4, y_off = 8, z_off = 2, w_off = 4;
    const int x_size = 16, y_size = 16, z_size = 3, w_size = 3;

    buffer_t cropped = *full.raw_buffer();
    cropped.host = (uint8_t *)&(full(x_off, y_off, z_off, w_off));
    cropped.min[0] = 0;
    cropped.min[1] = 0;
    cropped.min[2] = 0;
    cropped.min[3] = 0;
    cropped.extent[0] = x_size;
    cropped.extent[1] = y_size;
    cropped.extent[2] = z_size;
    cropped.extent[3] = w_size;
    cropped.stride[0] *= 2;
    cropped.stride[1] *= 2;
    cropped.stride[2] *= 2;
    cropped.stride[3] *= 2;
    Buffer out(Int(32), &cropped);

    // Make a bitmask representing the region inside the crop.
    Image<bool> in_subregion(80, 60, 10, 10);
    Expr test = ((x >= x_off) && (x < x_off + x_size*2) &&
                 (y >= y_off) && (y < y_off + y_size*2) &&
                 (z >= z_off) && (z < z_off + z_size*2) &&
                 (w >= w_off) && (w < w_off + w_size*2) &&
                 (x % 2 == 0) &&
                 (y % 2 == 0) &&
                 (z % 2 == 0) &&
                 (w % 2 == 0));
    Func test_func;
    test_func(x, y, z, w) = test;
    test_func.realize(in_subregion);

    Func f;
    f(x, y, z, w) = 3*x + 2*y + z + 4*w;
    f.gpu_tile(x, y, 16, 16);
    f.output_buffer().set_stride(0, Expr());
    f.realize(out);

    // Put some data in the full host buffer, avoiding the region
    // being evaluated above.
    Expr change_out_of_subregion = select(test, undef<int>(), 4*x + 3*y + 2*z + w);
    lambda(x, y, z, w, change_out_of_subregion).realize(full);

    // Copy back the output subset from the GPU.
    out.copy_to_host();

    for (int w = 0; w < full.extent(3); ++w) {
        for (int z = 0; z < full.extent(2); ++z) {
            for (int y = 0; y < full.extent(1); ++y) {
                for (int x = 0; x < full.extent(0); ++x) {
                    int correct;
                    if (in_subregion(x, y, z, w)) {
                        int x_ = (x - x_off)/2;
                        int y_ = (y - y_off)/2;
                        int z_ = (z - z_off)/2;
                        int w_ = (w - w_off)/2;
                        correct = 3*x_ + 2*y_ + z_ + 4*w_;
                    } else {
                        correct = 4*x + 3*y + 2*z + w;
                    }
                    if (full(x, y, z, w) != correct) {
                        printf("Error! Incorrect value %i != %i at %i, %i, %i, %i\n", full(x, y, z, w), correct, x, y, z, w);
                        return -1;
                    }
                }
            }
        }
    }

    printf("Success!\n");
    return 0;
}
