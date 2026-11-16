# Source this file from a Bash shell to enable PlatformIO's VS Code environment.
_pio_env_activate="$HOME/.platformio/penv/bin/activate"

if [[ -r "$_pio_env_activate" ]]; then
	source "$_pio_env_activate"
else
	_pio_env_bin="$HOME/.platformio/penv/bin"
	case ":$PATH:" in
		*":$_pio_env_bin:"*) ;;
		*) export PATH="$_pio_env_bin:$PATH" ;;
	esac
	unset _pio_env_bin
fi

unset _pio_env_activate
