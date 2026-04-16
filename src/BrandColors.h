#pragma once

namespace BrandColors {

/// Dark UI — same palette as https://datagateapp.com/ (:root in site CSS).
namespace Dark {
inline constexpr char kBgDefault[] = "#0d1117";
inline constexpr char kBgMuted[] = "#161b22";
inline constexpr char kBgSubtle[] = "#21262d";
inline constexpr char kBgInset[] = "#010409";
inline constexpr char kTextDefault[] = "#c9d1d9";
inline constexpr char kTextMuted[] = "#8b949e";
inline constexpr char kTextFaint[] = "#6e7681";
inline constexpr char kAccentFg[] = "#58a6ff";
inline constexpr char kAccentEmphasis[] = "#1f6feb";
inline constexpr char kAccentHover[] = "#388bfd";
inline constexpr char kBorderDefault[] = "#30363d";
inline constexpr char kBorderMuted[] = "#21262d";
inline constexpr char kSuccess[] = "#3fb950";
inline constexpr char kDanger[] = "#f85149";
inline constexpr char kWhite[] = "#ffffff";
} // namespace Dark

/// Light mode — not shipped on the marketing site; uses the same accent family for parity across Win/Linux.
namespace Light {
inline constexpr char kBgDefault[] = "#f6f8fa";
inline constexpr char kBgMuted[] = "#ffffff";
inline constexpr char kBgSubtle[] = "#f6f8fa";
inline constexpr char kBgInset[] = "#ffffff";
inline constexpr char kTextDefault[] = "#24292f";
inline constexpr char kTextMuted[] = "#656d76";
inline constexpr char kTextFaint[] = "#8c959f";
inline constexpr char kAccentFg[] = "#0969da";
inline constexpr char kAccentEmphasis[] = "#0969da";
inline constexpr char kAccentHover[] = "#0550ae";
inline constexpr char kBorderDefault[] = "#d0d7de";
inline constexpr char kBorderMuted[] = "#d8dee4";
inline constexpr char kWhite[] = "#ffffff";
} // namespace Light

} // namespace BrandColors
