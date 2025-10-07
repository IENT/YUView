import numpy as np

def format_c_array(arr, typeName, name):
    formatted = ', '.join(str(x) for x in arr)
    return f"constexpr std::array<{typeName}, NR_SAMPLES> {name} = {{ {formatted} }};"
    
for bitDepth in [8, 10, 12, 16]:
    maxValue = 2 ** bitDepth - 1
    midValue = (maxValue + 1) // 2

    array1 = np.random.randint(0, maxValue + 1, 48)
    array2 = np.random.randint(0, maxValue + 1, 48)

    # Add some special values that we definitely want to test
    array1[0] = 0
    array2[0] = maxValue
    array1[1] = maxValue
    array2[1] = 0
    array1[2] = maxValue
    array2[2] = maxValue

    array1[3] = 0
    array2[3] = 0
    array1[4] = midValue
    array2[4] = 0
    array1[5] = 0
    array2[5] = midValue

    array1[6] = 0
    array2[6] = midValue - 1
    array1[7] = 0
    array2[7] = midValue + 1
    array1[8] = 0
    array2[8] = midValue - 1
    
    array1[9] = midValue
    array2[9] = maxValue
    array1[10] = 44
    array2[10] = maxValue

    typeName = f"uint8_t" if bitDepth == 8 else f"uint16_t"
    print(format_c_array(array1, typeName, f"RAW_DATA_A_{bitDepth}BIT"))
    print(format_c_array(array2, typeName, f"RAW_DATA_B_{bitDepth}BIT"))

    # Calculate the "normal" difference array
    diff_array = (array1 - array2) + 128
    diff_array = np.clip(diff_array, 0, 255)
    print(format_c_array(diff_array, "uint8_t", f"DIFF_ARRAY_{bitDepth}BIT"))

    # Calculate some scaled difference arrays
    for scale in [2, 5]:
        scaled_diff_array = ((array1 - array2) * scale + midValue)
        scaled_diff_array = np.clip(scaled_diff_array, 0, 255)
        print(format_c_array(scaled_diff_array, "uint8_t", f"DIFF_SCALED_ARRAY_{bitDepth}BIT_SCALE{scale}"))

    # Calculate the "mark difference" results
    mark_diff_array = np.where((array1 - array2) != 0, 255, 0)
    print(format_c_array(mark_diff_array, "uint8_t", f"MARK_DIFF_ARRAY_{bitDepth}BIT"))
