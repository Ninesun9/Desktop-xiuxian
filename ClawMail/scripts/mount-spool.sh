#!/bin/bash
# RobotMail SSHFS 挂载脚本
# 自动挂载远程 spool 到本地

REMOTE_HOST="root@107.174.220.99"
LOCAL_MOUNT="/robotmail/spool"
FSTAB_LINE="$REMOTE_HOST:/robotmail/spool $LOCAL_MOUNT fuse.sshfs defaults,_netdev,IdentityFile=~/.ssh/id_ed25519 0 0"

# 检查是否已挂载
is_mounted() {
    mountpoint -q "$LOCAL_MOUNT" 2>/dev/null
}

# 挂载
mount_spool() {
    if is_mounted; then
        echo "✅ 已挂载: $LOCAL_MOUNT"
        return 0
    fi
    
    echo "🔗 正在挂载 $REMOTE_HOST..."
    
    # 确保目录存在
    mkdir -p "$LOCAL_MOUNT"
    
    # 尝试挂载
    if sshfs -o allow_other,default_permissions,IdentityFile=~/.ssh/id_ed25519 "$REMOTE_HOST:/robotmail/spool" "$LOCAL_MOUNT"; then
        echo "✅ 挂载成功: $LOCAL_MOUNT"
        return 0
    else
        echo "❌ 挂载失败"
        return 1
    fi
}

# 卸载
umount_spool() {
    if ! is_mounted; then
        echo "ℹ️ 未挂载"
        return 0
    fi
    
    echo "🔌 正在卸载..."
    if fusermount -u "$LOCAL_MOUNT"; then
        echo "✅ 已卸载"
        return 0
    else
        echo "❌ 卸载失败"
        return 1
    fi
}

# 添加到 fstab
add_to_fstab() {
    if grep -q "$LOCAL_MOUNT" /etc/fstab 2>/dev/null; then
        echo "ℹ️ fstab 已存在"
        return 0
    fi
    
    echo "$FSTAB_LINE" >> /etc/fstab
    echo "✅ 已添加到 fstab"
}

# 主命令
case "${1:-status}" in
    mount)
        mount_spool
        ;;
    umount|unmount)
        umount_spool
        ;;
    add-fstab)
        add_to_fstab
        ;;
    status)
        if is_mounted; then
            echo "✅ 已挂载: $LOCAL_MOUNT"
            df -h "$LOCAL_MOUNT" 2>/dev/null | tail -1
        else
            echo "❌ 未挂载: $LOCAL_MOUNT"
        fi
        ;;
    *)
        echo "用法: $0 {mount|umount|add-fstab|status}"
        exit 1
        ;;
esac
