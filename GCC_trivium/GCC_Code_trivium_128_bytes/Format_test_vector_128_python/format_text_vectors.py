'''
 Simple program to formatting copy-pasted (with unnecessary characters) text to another file. Here are thr description
 for the all files in the project folder
 [all_test_vectors.txt]      :- consists of all copy-pasted text with unnecessary characters
 [test_vectors.txt]          :- consists of manually formatted text- using for testing
 [formatted_text_vectors.txt]:- this program will dump the all formatted text into this file
 [check_similarity.py]       :- a test case for first two text
'''

# parameters
check_errors = 1
file_read = 0
file_write = 0
number_of_entries = 4

# scan files
try:
    file_read = open('all_test_vectors.txt', "r")
    file_write = open('formatted_text_vectors.txt', "w")
except (FileNotFoundError, IOError):
    print("[ERROR] wrong file or file path")
    exit(0)

lines = file_read.readlines()

# check the lines
length_float = len(lines)/number_of_entries
length_int = int(length_float)

# print a error message if there are additional line (copy-paste error)
if (length_float - length_int) != 0.0:
    print("[ERROR] error due to lines in all_test_vectors.txt ")
else:
    for i in range(length_int):
        formatted_lines = []
        for line in range(0, number_of_entries):
            empty_list = []

            empty_list += lines[i*number_of_entries + line]
            split_from_equal = []

            # remove the 'stream[]' part from the text vectors
            for local_data in empty_list:
                if local_data == '=':
                    split_from_equal = lines[i*number_of_entries + line].split(' = ')
            if len(split_from_equal) == 0:
                formatted_lines += lines[i*number_of_entries + line]
            else:
                formatted_lines += split_from_equal[1]

        for data in formatted_lines:
            if data == '\n':
                formatted_lines.remove(data)

        # string now has only '.', ' ' as unnecessary part
        # remove '.'
        for data in formatted_lines:
            if data == '.':
                formatted_lines.remove(data)

        # print(formatted_lines)

        # concatenate the strings in to one string by removing ' '
        key_string_128_bytes = ''.join(formatted_lines)
        key_string_128_bytes = key_string_128_bytes.replace(' ', '')

        print(key_string_128_bytes)
        # write the string in to new file
        file_write.write(key_string_128_bytes)
        file_write.write("\n")

    # close the file after reading the lines.
    file_read.close()
    file_write.close()

    # test the similarity of the manually edited file and created file using this script
    if check_errors:
        import check_similarity
