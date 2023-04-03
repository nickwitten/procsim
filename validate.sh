#!/bin/bash
set -e

student_stat_dir=student_outs
benchmarks=( bfs_2_15 cachesim_gcc perceptron_gcc tiledmm )
configs=( big med med_nomiss tiny )

flags_big='-P 128 -F 8 -S 8 -A 3 -M 2 -L 3'
flags_med='-P 96 -F 4 -S 4 -A 2 -M 1 -L 2'
flags_med_nomiss='-P 96 -F 4 -S 4 -A 2 -M 1 -L 2 -D'
flags_tiny='-P 64 -F 2 -S 2 -A 1 -M 1 -L 1'

name_big=Big
name_med=Medium
name_med_nomiss='Medium without any misses'
name_tiny=Tiny

banner() {
    local message=$1
    printf '%s\n' "$message"
    yes = | head -n ${#message} | tr -d '\n'
    printf '\n'
}

student_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "${student_stat_dir}/${config}_${benchmark}.out"
}

ta_stat_path() {
    local config=$1
    local benchmark=$2

    printf '%s' "ref_outs/${config}_${benchmark}.out"
}

benchmark_path() {
    benchmark=$1
    printf '%s' "traces/${benchmark}_full.trace"
}

human_friendly_config() {
    local config=$1
    local name_var=name_$config
    printf '%s' "${!name_var}"
}

generate_stats() {
    local config=$1
    local benchmark=$2

    local flags_var=flags_$config
    if ! bash run.sh ${!flags_var} -I "$(benchmark_path "$benchmark")" &>"$(student_stat_path "$config" "$benchmark")"; then
        printf 'Exited with nonzero status code. Please check the log file %s for details\n' "$(student_stat_path "$config" "$benchmark")"
    fi
}

generate_stats_and_diff() {
    local config=$1
    local benchmark=$2

    printf '==> Running %s... ' "$benchmark"
    generate_stats "$config" "$benchmark"
    if diff -u "$(ta_stat_path "$config" "$benchmark")" "$(student_stat_path "$config" "$benchmark")"; then
        printf 'Matched!\n'
    else
        local flags_var=flags_$config
        printf '\nPlease examine the differences printed above. Benchmark: %s. Config name: %s. Flags to procsim used: %s\n\n' "$benchmark" "$config" "${!flags_var} -I $(benchmark_path "$benchmark")"
    fi
}

main() {
    mkdir -p "$student_stat_dir"

    if [[ $# -gt 0 ]]; then
        local use_configs=( "$@" )

        for config in "${use_configs[@]}"; do
            local flags_var=flags_$config
            if [[ -z ${!flags_var} ]]; then
                printf 'Unknown configuration %s. Available configurations:\n' "$config"
                printf '%s\n' "${configs[@]}"
                return 1
            fi
        done
    else
        local use_configs=( "${configs[@]}" )
    fi

    local first=1
    for config in "${use_configs[@]}"; do
        if [[ $first -eq 0 ]]; then
            printf '\n'
        else
            local first=0
        fi

        banner "Testing $(human_friendly_config "$config")..."

        local flags_var=flags_$config
        printf 'procsim flags: %s\n\n' "${!flags_var}"

        for benchmark in "${benchmarks[@]}"; do
            generate_stats_and_diff "$config" "$benchmark"
        done
    done
}

main "$@"
