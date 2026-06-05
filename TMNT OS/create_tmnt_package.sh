#!/bin/bash
# create_tmnt_package.sh - Now with game support!

APP_NAME="$1"
VERSION="$2"
CATEGORY="$3"  # 0=Utility, 1=Game, 2=Theme, 3=Tool
DESCRIPTION="$4"
EXECUTABLE="$5"  # e.g., "startup" or "main" or "game.bin"
APP_DIR="$6"
OUTPUT="${APP_NAME}.tmnt"

if [ $# -lt 6 ]; then
    echo "Usage: $0 <name> <version> <category> <description> <executable> <app_dir>"
    echo "Categories: 0=Utility, 1=Game, 2=Theme, 3=Tool"
    echo "Example: $0 'TurtleNinja' '1.0' 1 'A ninja turtle game!' 'game.bin' ./mygame/"
    exit 1
fi

TEMP_FILE=$(mktemp)

# Magic "TMNT"
printf "TMNT" > "$TEMP_FILE"

# File count
FILE_COUNT=$(find "$APP_DIR" -type f | wc -l)
printf "$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' \
    $((FILE_COUNT & 0xFF)) $(((FILE_COUNT >> 8) & 0xFF)) \
    $(((FILE_COUNT >> 16) & 0xFF)) $(((FILE_COUNT >> 24) & 0xFF)))" >> "$TEMP_FILE"

# App name
NAME_LEN=${#APP_NAME}
printf "\\x$(printf '%02x' $NAME_LEN)" >> "$TEMP_FILE"
printf "%s" "$APP_NAME" >> "$TEMP_FILE"

# Description
DESC_LEN=${#DESCRIPTION}
printf "\\x$(printf '%02x' $DESC_LEN)" >> "$TEMP_FILE"
printf "%s" "$DESCRIPTION" >> "$TEMP_FILE"

# Version
VER_LEN=${#VERSION}
printf "\\x$(printf '%02x' $VER_LEN)" >> "$TEMP_FILE"
printf "%s" "$VERSION" >> "$TEMP_FILE"

# Executable path
EXE_LEN=${#EXECUTABLE}
printf "\\x$(printf '%02x' $EXE_LEN)" >> "$TEMP_FILE"
printf "%s" "$EXECUTABLE" >> "$TEMP_FILE"

# Category
printf "\\x$(printf '%02x' $CATEGORY)" >> "$TEMP_FILE"

# Add files
cd "$APP_DIR"
find . -type f | while read -r FILE; do
    FILEPATH="${FILE#./}"
    FILEPATH_LEN=${#FILEPATH}
    
    printf "\\x$(printf '%02x' $FILEPATH_LEN)" >> "$TEMP_FILE"
    printf "%s" "$FILEPATH" >> "$TEMP_FILE"
    
    FILESIZE=$(stat -c%s "$FILE")
    printf "$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' \
        $((FILESIZE & 0xFF)) $(((FILESIZE >> 8) & 0xFF)) \
        $(((FILESIZE >> 16) & 0xFF)) $(((FILESIZE >> 24) & 0xFF)))" >> "$TEMP_FILE"
    
    cat "$FILE" >> "$TEMP_FILE"
done
cd - > /dev/null

mv "$TEMP_FILE" "$OUTPUT"
echo "✅ Created $OUTPUT - $APP_NAME v$VERSION ($([ "$CATEGORY" = "1" ] && echo "🎮 GAME" || echo "📦 APP"))"