/*
 * Copyright (C) 2026 Toilettrauma
 */
#pragma once

#include <aniparse/Parser.hpp>

#include <memory>

std::shared_ptr<aniparse::Parser> make_benchmark_parser(int number);
