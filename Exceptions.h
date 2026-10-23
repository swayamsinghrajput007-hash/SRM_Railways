#pragma once
#include <stdexcept>
#include <string>
struct RailwayException : public std::runtime_error { using std::runtime_error::runtime_error; };
struct StationNotOnRouteException : RailwayException { using RailwayException::RailwayException; };
struct AdvanceBookingException : RailwayException { using RailwayException::RailwayException; };
struct ChartPreparedException : RailwayException { using RailwayException::RailwayException; };
struct TatkalNotOpenException : RailwayException { using RailwayException::RailwayException; };
struct BerthNotAvailableException : RailwayException { using RailwayException::RailwayException; };
