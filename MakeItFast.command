#!/usr/bin/env bash
# Make-Mac-Fast.command — reduce UI animations/transparency in macOS

log() { echo "[$(date +%H:%M:%S)] $*"; }

write_pref() {
  local domain="$1" key="$2" type="$3" value="$4"
  if defaults write "$domain" "$key" "-$type" "$value" 2>/dev/null; then
    log "Set $domain:$key -> $value"
  else
    # Fallback: write directly to user's plist
    local plist="$HOME/Library/Preferences/${domain}.plist"
    if defaults write "$plist" "$key" "-$type" "$value" 2>/dev/null; then
      log "Set (plist) $plist:$key -> $value"
    else
      log "Warning: couldn't set $domain:$key"
    fi
  fi
}

log "Applying low-animation settings…"

# Accessibility (system-wide)
write_pref com.apple.universalaccess reduceMotion bool true
write_pref com.apple.universalaccess reduceTransparency bool true

# Global UI animations
write_pref -g NSAutomaticWindowAnimationsEnabled bool false
write_pref -g NSWindowResizeTime float 0.001
write_pref -g NSScrollAnimationEnabled bool false

# Finder animations
write_pref com.apple.finder DisableAllAnimations bool true

# Dock: scale minimize, faster show/hide, no launch bounce
write_pref com.apple.dock mineffect string "scale"
write_pref com.apple.dock launchanim bool false
write_pref com.apple.dock autohide-delay float 0
write_pref com.apple.dock autohide-time-modifier float 0.12
write_pref com.apple.dock expose-animation-duration float 0.1

log "Restarting UI services…"
killall Dock 2>/dev/null
killall Finder 2>/dev/null
killall SystemUIServer 2>/dev/null

osascript -e 'display notification "Reduced animations and transparency applied." with title "Mac UI Tweaks"'
log "Done."
exit 0
