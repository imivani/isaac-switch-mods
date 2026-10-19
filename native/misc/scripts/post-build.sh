#!/bin/bash
set -e

OUT_NSO=${OUT}/${BINARY_NAME}
# Keep the separately validated, minimally changed original descriptor.
mkdir -p "${OUT}"

# Copy build into out
cp "${NAME}.nso" "${OUT_NSO}"

# Copy ELF to user path if defined.
if [ ! -z $ELF_EXTRACT ]; then
    cp "$NAME.elf" "$ELF_EXTRACT"
fi
