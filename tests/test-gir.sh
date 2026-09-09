#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Force a fresh scanner run so cached artifacts cannot hide annotation defects.
set -euo pipefail
case "${1:-}" in
	-h|--help) printf '%s\n' 'Usage: tests/test-gir.sh [--help|--license]' 'Example: tests/test-gir.sh'; exit 0 ;;
	--license) printf '%s\n' 'AGPL-3.0-or-later'; exit 0 ;;
	'') ;;
	*) printf '%s\n' 'Unknown argument' >&2; exit 2 ;;
esac
export GI_SCANNER_DISABLE_CACHE=1
make -W src/mcp-root.h GIR_SCANNER='g-ir-scanner --warn-error' gir

# Never fix scanner failures by making public callables disappear from GI.
if rg -U '<(function|method|constructor|callback|virtual-method)[[:space:]][^>]*introspectable="0"' build/Mcp-1.0.gir
then
	printf '%s\n' 'Public callable is not introspectable' >&2
	exit 1
fi
for record in ToolResult ResourceContents PromptMessage PromptResult SamplingMessage ModelPreferences SamplingResult Root CompletionResult
do
	rg -U -q "<record name=\"${record}\"[^>]*glib:get-type=" build/Mcp-1.0.gir
done
rg -U -q '<enumeration name="ErrorCode"[^>]*glib:error-domain=' build/Mcp-1.0.gir
# Registered handlers retain their closure until destroy notify runs.
rg -U -q '<parameter name="handler"[^>]*scope="notified"[^>]*closure="2"[^>]*destroy="3"' build/Mcp-1.0.gir
# Sampling preferences must expose their record instead of an opaque pointer.
rg -U -q '<parameter name="model_preferences"[^>]*>\s*<doc[^>]*>[^<]*</doc>\s*<type name="ModelPreferences"' build/Mcp-1.0.gir
