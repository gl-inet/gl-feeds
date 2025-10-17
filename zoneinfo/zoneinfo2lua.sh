#!/bin/bash

# IANA时区数据库目录
ZONEINFO_DIR="./zoneinfo"

# 各城市/地区时区链接记录文件
BACKWARD_FILE="../backward"

# 保存原始数据
ZONE_TAB="./zone_tab.lua"
LINK_FILE="./link_tab.lua"

# 保存最终数据
OUTPUT_FILE=$1

# zoneinfo时区文件格式
ZONEINFO_FORMAT="TZif"

> $ZONE_TAB
> $LINK_FILE
> $OUTPUT_FILE

generate_zone_tab() {
    # 遍历zoneinfo目录下的所有文件
    find "$ZONEINFO_DIR" -type f | sort -k1V | while read -r file; do
        # 提取相对于zoneinfo目录的路径
        relative_path=$(printf "$file" | sed "s|$ZONEINFO_DIR/||")
        # 提取文件的第一行内容
        file_type=$(head -1 "$file" | cut -b 1-4)

        if [ "$file_type" = "$ZONEINFO_FORMAT" ]; then
            # 提取文件的最后一行内容
            last_line=$(tail -1 "$file")
            # 组装文件路径和最后一行内容，并写入$ZONE_TAB文件
            printf "$relative_path\t$last_line\n" >> "$ZONE_TAB"
        fi
    done
    grep "^Etc" "$ZONE_TAB" > "./tmp.lua" && grep -v "^Etc" "$ZONE_TAB" >> "./tmp.lua" && mv "./tmp.lua" "$ZONE_TAB"
}

generate_link_tab() {
    skip_line_start=$(sed -n "/# Pre-2013 practice, which typically had a Zone per zone.tab line/=" "$BACKWARD_FILE")
    skip_line_end=$(sed -n "/# Non-zone.tab locations with timestamps since 1970 that duplicate/=" "$BACKWARD_FILE")
    current_line=0

    # 提取backward文件中各行的第3列字段
    while IFS= read -r line; do
        current_line=$(($current_line + 1))
        if [ $current_line -ge $skip_line_start ] && [ $current_line -le $skip_line_end ]; then
            continue
        fi
        # 提取每行的第2-3字段
        field2=$(printf "$line" | awk '$1 == "Link" {print $2}')
        field3=$(printf "$line" | awk '$1 == "Link" {print $3}')

        # 如果field2 3不为空，则将其写入输出文件
        if [ -n "$field2" ] && [ -n "$field3" ]; then
            printf "$field2\t$field3\n" >> "$LINK_FILE"

            field3=$(printf "$field3" | sed "s#\/#\\\/#g")
            sed -i "/^$field3/d" "$ZONE_TAB"
        fi
    done < "$BACKWARD_FILE"

    sed -i -e "/^GMT/d" -e "/^WET/d" -e "/^Factory/d" -e "/^Etc\/UTC/d" "$ZONE_TAB"
    sort "$LINK_FILE" > "./tmp.lua" && mv "./tmp.lua" "$LINK_FILE"
}

generate_zoneinfo_tab() {
    declare -A zonename_groups
    while IFS=$'\t' read -r zonename1 zonename2; do
        if [[ -n "$zonename1" && -n "$zonename2" ]]; then
            if [[ -z "${zonename_groups[$zonename1]}" ]]; then
                zonename_groups[$zonename1]="'$zonename2'"
            else
                zonename_groups[$zonename1]="${zonename_groups[$zonename1]}, '$zonename2'"
            fi
        fi
    done < "$LINK_FILE"

    # 获取所有zonename1的键，并排序
    keys=(${!zonename_groups[@]})
    IFS=$'\n' sorted_keys=($(sort <<<"${keys[*]}"))

    # 生成zoneinfo_tab表
    printf "local M = {}\n\n" >> "$OUTPUT_FILE"
    printf "M.zoneinfo_tab = {\n" >> "$OUTPUT_FILE"
    printf "\t{ zonename = 'UTC', timezone = 'UTC' },\n" | sed "s/_/ /g" >> "$OUTPUT_FILE"
    while IFS=$'\t' read -r zonename timezone; do
        if [[ -n "$zonename" && -n "$timezone" ]]; then
            if [[ -z "${zonename_groups[$zonename]}" ]]; then
                printf "\t{ zonename = '$zonename', timezone = '$timezone' },\n" | sed "s/_/ /g" >> "$OUTPUT_FILE"
            else
                printf "\t{ zonename = '$zonename', timezone = '$timezone', links = { '$zonename', ${zonename_groups[$zonename]} } },\n" | sed "s/_/ /g" >> "$OUTPUT_FILE"
            fi
        fi
    done < "$ZONE_TAB"
    printf "}\n\n" >> "$OUTPUT_FILE"
    printf "return M\n" >> "$OUTPUT_FILE"
}

generate_zone_tab
generate_link_tab
generate_zoneinfo_tab

echo "Success!"

