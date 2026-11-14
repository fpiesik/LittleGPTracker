#include "Application/Utils/SampleTransition.h"
#include <cassert>
#include <climits>
#include <cstdio>
#include <vector>

int main() {
    const int rates[] = {22050, 44100, 48000, 96000};
    for (int r = 0; r < 4; ++r) {
        int n = rates[r] / 500;
        SampleTransition whole, split;
        fixed previous[] = {INT_MAX, INT_MIN};
        whole.Process(previous, 1, rates[r], false);
        split.Process(previous, 1, rates[r], false);
        std::vector<fixed> a(2 * (n + 4)), b;
        for (int i = 0; i < n + 4; ++i) {
            a[2 * i] = INT_MIN;
            a[2 * i + 1] = INT_MAX;
        }
        b = a;
        whole.Process(&a[0], n + 4, rates[r], true);
        for (int i = 0; i < n + 4; ++i)
            split.Process(&b[2 * i], 1, rates[r], i == 0);
        assert(a == b); // Buffer partitioning must not change the sound.
        assert(a[0] == previous[0] && a[1] == previous[1]);
        for (int i = 1; i <= n; ++i) {
            assert(a[2 * i] <= a[2 * (i - 1)]);
            assert(a[2 * i + 1] >= a[2 * (i - 1) + 1]);
        }
        assert(a[2 * n] == INT_MIN && a[2 * n + 1] == INT_MAX);
        fixed jump[] = {100, -200};
        whole.Process(jump, 1, rates[r], true);
        fixed rapid[] = {300, -400};
        whole.Process(rapid, 1, rates[r], true);
        assert(rapid[0] == jump[0] && rapid[1] == jump[1]);
        whole.Reset();
        whole.Process(rapid, 1, rates[r], true);
        assert(rapid[0] == 0 && rapid[1] == 0);
        whole.Reset();
        fixed untouched[] = {1234567, -7654321};
        whole.Process(untouched, 1, rates[r], false);
        assert(untouched[0] == 1234567 && untouched[1] == -7654321);
    }
    puts("Sample transition tests passed");
}
