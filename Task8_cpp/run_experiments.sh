#!/bin/bash

FILES=("resources/simplewiki-20260201.txt" "resources/1brs-traj1.pdb" "resources/400mb.fa")

for FILE in "${FILES[@]}"; do
    echo "========================================"
    echo "Processing: $FILE"
    
    COMPRESSED="${FILE}.compressed"
    DECOMPRESSED="${FILE}.decompressed"

    # Сжатие
    ./mycompressor.exe -c "$FILE" "$COMPRESSED"
    
    # Распаковка
    ./mycompressor.exe -d "$COMPRESSED" "$DECOMPRESSED"
    
    # Проверка целостности (diff не должен ничего вывести)
    if cmp -s "$FILE" "$DECOMPRESSED"; then
        echo "Integrity check: PASS (Files are identical)"
    else
        echo "Integrity check: FAIL"
    fi
    echo "========================================"
done