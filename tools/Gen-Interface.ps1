<#
.SYNOPSIS
Prints the declaration skeleton of an nvrhi::core interface or implementation class with a fresh GUID.

.DESCRIPTION
QueryInterface is explicit (nvrhi ADR 0007): an interface answers only the IIDs that a class lists in its
interface table, so a class lists every interface it implements, ancestors included, and its class ID.
-Bases are the ObjectImpl bases (the interfaces the class derives from); -Ancestors are the interfaces those
derive from (other than nvrhi::IObject), listed in the table only.

.EXAMPLE
.\Gen-Interface.ps1 -InterfaceName IFoo                       # struct IFoo : nvrhi::IObject
.\Gen-Interface.ps1 -InterfaceName IBar -Parent IFoo           # struct IBar : IFoo
.\Gen-Interface.ps1 -ClassName Foo -Bases IBar -Ancestors IFoo # class Foo final : public nvrhi::ObjectImpl<IBar>
#>
[CmdletBinding(DefaultParameterSetName = 'Interface')]
param(
    [Parameter(Mandatory, HelpMessage = "Interface Name", ParameterSetName = 'Interface')]
    [string]$InterfaceName,
    [Parameter(HelpMessage = "Parent interface", ParameterSetName = 'Interface')]
    [string]$Parent = 'nvrhi::IObject',
    [Parameter(Mandatory, HelpMessage = "Class Name", ParameterSetName = 'Class')]
    [string]$ClassName,
    [Parameter(HelpMessage = "ObjectImpl bases (interfaces the class implements)", ParameterSetName = 'Class')]
    [string[]]$Bases = @('nvrhi::IObject'),
    [Parameter(HelpMessage = "Ancestor interfaces of the bases (listed in the table, not inherited again)", ParameterSetName = 'Class')]
    [string[]]$Ancestors = @()
)

# The GUID parser only accepts lowercase hex digits.
$GuidLiteral = (New-Guid).ToString('D').ToLowerInvariant()

if ($InterfaceName) {
$Note = if ($Parent -in 'nvrhi::IObject', 'IObject') { "lists it in its interface table" } else { "lists it and its ancestors ($Parent, ...) in its interface table" }
Write-Output @"
NVRHI_IID($InterfaceName, `"$GuidLiteral`")
struct $InterfaceName : $Parent
{
    NVRHI_DECLARE_UUID_TRAITS($InterfaceName)
    // A class implementing $InterfaceName $Note.
};
"@
} else {
$BaseList = $Bases -join ', '
$Entries = (@($Bases) + @($Ancestors | Where-Object { $_ })) | Select-Object -Unique |
    ForEach-Object { "    NVRHI_IMPLEMENTS_INTERFACE($_)" }
$EntryText = $Entries -join "`r`n"
# NVRHI_CLASS_CLSID forward-declares the class and gives it the class ID; NVRHI_IMPLEMENTS_CLASS answers it
# (what checked_cast and the QueryInterface type tests use, nvrhi ADR 0006).
Write-Output @"
NVRHI_CLASS_CLSID($ClassName, `"$GuidLiteral`")
class $ClassName final : public nvrhi::ObjectImpl<$BaseList>
{
public:
    NVRHI_DECLARE_UUID_TRAITS($ClassName)

    // Explicit interface table (nvrhi ADR 0007). List EVERY interface the class implements, ancestors
    // included: an ancestor that is not listed is not answered. Check that the parents of the bases
    // ($BaseList) are all here (e.g. nvrhi::IRHIObject for an NVRHI resource interface). The first entry
    // answers IObject; NVRHI_IMPLEMENTS_CLASS answers the class ID.
    NVRHI_BEGIN_INTERFACE_TABLE_INLINE($ClassName)
$EntryText
    NVRHI_IMPLEMENTS_CLASS($ClassName)
    NVRHI_END_INTERFACE_TABLE()
};
"@
}
