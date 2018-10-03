# Here 'test_vectors.txt' is a manually fomacted file and 'formacted_test_vectors.txt' is edited using the script
# Following code will check the similarity of these two files up to line numbers in manually edited file ('similarity_check.txt')

# open files
file1 = open('similarity_check.txt', "r")
file2 = open('formatted_text_vectors.txt', "r")
# read files
line_file1 = file1.readlines()
line_file2 = file2.readlines()

for i in range(0, len(line_file1)):
    if line_file1[i] != line_file2[i]:
        print('[ERROR]:Line number', i, "is not equal")
    else:
        print('[SUCCESS]')
