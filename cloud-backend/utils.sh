#!/bin/zsh

# Logging functions
log_info() {
    echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

log_error() {
    echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') - $1" >&2
}

# Calculate md5sum for first parameter
# 
# Use different builtin binary depending on OS
get_md5sum () {
    case "$OSTYPE" in
        linux*)   echo -n "$1" | md5sum ;;
        darwin*)  echo -n "$1" | md5 ;;
    esac
}

# Get stack name based on current git branch
get_stack_name() {
    local branch_name=$(git branch --show-current)
    # alternatively:
    # branch_name=$(git rev-parse --abbrev-ref HEAD)

    if [[ "$branch_name" == "main" ]]; then
        echo "smart-house"
    else
        # unfortunately 'git rev-parse $branch_name' gives hash of last commit
        local branch_hash="$(get_md5sum "$branch_name")"
        echo "smart-house-${branch_hash:0:7}-dev"
        # alternatively:
        # branch_hash=$(echo -n $branch_name | md5 | cut -c1-7) # first 7 characters
        # stack_name="polish-caves-$branch_hash-dev"
    fi
}
