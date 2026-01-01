#ifndef SEGMENTTREE_H
#define SEGMENTTREE_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

struct SegmentStats {
    double total_distance;      
    double total_fuel;          
    double total_expenses;     
    double avg_speed;           
    int trip_count;
    double max_speed;
    double min_speed;
    int harsh_braking_events;
    int speeding_violations;
    int days_covered;           
    
    SegmentStats() : total_distance(0), total_fuel(0), total_expenses(0),
                     avg_speed(0), trip_count(0), max_speed(0), 
                     min_speed(999999), harsh_braking_events(0),
                     speeding_violations(0), days_covered(0) {}
    
    static SegmentStats merge(const SegmentStats& left, const SegmentStats& right) {
        SegmentStats result;
        
        result.total_distance = left.total_distance + right.total_distance;
        result.total_fuel = left.total_fuel + right.total_fuel;
        result.total_expenses = left.total_expenses + right.total_expenses;
        result.trip_count = left.trip_count + right.trip_count;
        result.harsh_braking_events = left.harsh_braking_events + right.harsh_braking_events;
        result.speeding_violations = left.speeding_violations + right.speeding_violations;
        result.days_covered = left.days_covered + right.days_covered;
        
        if (result.trip_count > 0) {
            result.avg_speed = (left.avg_speed * left.trip_count + 
                               right.avg_speed * right.trip_count) / result.trip_count;
        }
        
        result.max_speed = std::max(left.max_speed, right.max_speed);
        result.min_speed = std::min(left.min_speed, right.min_speed);
        
        return result;
    }
};

struct SegmentTreeNode {
    int start_day;         
    int end_day;            
    SegmentStats stats;
    int left_child;         
    int right_child;
    
    SegmentTreeNode() : start_day(0), end_day(0), left_child(-1), right_child(-1) {}
};

class SegmentTree {
private:
    std::vector<SegmentTreeNode> tree_;
    int num_days_;
    uint64_t base_date_;   
    
    int build(const std::vector<SegmentStats>& daily_stats, int start, int end) {
        int node_idx = tree_.size();
        tree_.emplace_back();
        
        tree_[node_idx].start_day = start;
        tree_[node_idx].end_day = end;
        
        if (start == end) {
            
            if (start < daily_stats.size()) {
                tree_[node_idx].stats = daily_stats[start];
            }
        } else {
            
            int mid = (start + end) / 2;
            tree_[node_idx].left_child = build(daily_stats, start, mid);
            tree_[node_idx].right_child = build(daily_stats, mid + 1, end);
            
            tree_[node_idx].stats = SegmentStats::merge(
                tree_[tree_[node_idx].left_child].stats,
                tree_[tree_[node_idx].right_child].stats
            );
        }
        
        return node_idx;
    }
    
    SegmentStats query_recursive(int node_idx, int query_start, int query_end) {
        if (node_idx == -1) return SegmentStats();
        
        SegmentTreeNode& node = tree_[node_idx];
        
        if (node.end_day < query_start || node.start_day > query_end) {
            return SegmentStats();
        }
        
        if (query_start <= node.start_day && node.end_day <= query_end) {
            return node.stats;
        }
        
        SegmentStats left_stats = query_recursive(node.left_child, query_start, query_end);
        SegmentStats right_stats = query_recursive(node.right_child, query_start, query_end);
        
        return SegmentStats::merge(left_stats, right_stats);
    }
    
    void update_recursive(int node_idx, int day, const SegmentStats& new_stats) {
        if (node_idx == -1) return;
        
        SegmentTreeNode& node = tree_[node_idx];
        
        if (node.start_day == node.end_day) {
            
            node.stats = new_stats;
            return;
        }
        
        int mid = (node.start_day + node.end_day) / 2;
        
        if (day <= mid) {
            update_recursive(node.left_child, day, new_stats);
        } else {
            update_recursive(node.right_child, day, new_stats);
        }
        
        node.stats = SegmentStats::merge(
            tree_[node.left_child].stats,
            tree_[node.right_child].stats
        );
    }

public:
    SegmentTree(int num_days, uint64_t base_date) 
        : num_days_(num_days), base_date_(base_date) {
        tree_.reserve(4 * num_days); 
    }
    
    void build(const std::vector<SegmentStats>& daily_stats) {
        tree_.clear();
        num_days_ = daily_stats.size();
        build(daily_stats, 0, num_days_ - 1);
    }
    
    SegmentStats query_range(int start_day, int end_day) {
        if (start_day < 0) start_day = 0;
        if (end_day >= num_days_) end_day = num_days_ - 1;
        if (start_day > end_day) return SegmentStats();
        
        return query_recursive(0, start_day, end_day);
    }
    
    SegmentStats query_by_timestamp(uint64_t start_ts, uint64_t end_ts) {
        const uint64_t SECONDS_PER_DAY = 86400;
        
        int start_day = (start_ts - base_date_) / SECONDS_PER_DAY;
        int end_day = (end_ts - base_date_) / SECONDS_PER_DAY;
        
        return query_range(start_day, end_day);
    }
    
    void update_day(int day, const SegmentStats& stats) {
        if (day < 0 || day >= num_days_) return;
        update_recursive(0, day, stats);
    }
    
    void update_by_timestamp(uint64_t timestamp, const SegmentStats& stats) {
        const uint64_t SECONDS_PER_DAY = 86400;
        int day = (timestamp - base_date_) / SECONDS_PER_DAY;
        update_day(day, stats);
    }
    
    SegmentStats get_all_stats() {
        return tree_.empty() ? SegmentStats() : tree_[0].stats;
    }
    
    SegmentStats get_monthly_stats(int year, int month) {
        int days_per_month = 30;
        int month_offset = (year - 2024) * 12 + (month - 1);
        int start_day = month_offset * days_per_month;
        int end_day = start_day + days_per_month - 1;
        
        return query_range(start_day, end_day);
    }
    
    SegmentStats get_quarterly_stats(int year, int quarter) {
        int days_per_quarter = 90;
        int quarter_offset = (year - 2024) * 4 + (quarter - 1);
        int start_day = quarter_offset * days_per_quarter;
        int end_day = start_day + days_per_quarter - 1;
        
        return query_range(start_day, end_day);
    }
    
    SegmentStats get_yearly_stats(int year) {
        int days_per_year = 365;
        int year_offset = year - 2024;
        int start_day = year_offset * days_per_year;
        int end_day = start_day + days_per_year - 1;
        
        return query_range(start_day, end_day);
    }
    
    int get_num_days() const { return num_days_; }
    uint64_t get_base_date() const { return base_date_; }
    size_t get_tree_size() const { return tree_.size(); }
};

#endif